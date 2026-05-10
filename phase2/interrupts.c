#include "interrupts.h"

#include <uriscv/liburiscv.h>

#include "pcb.h"
#include "scheduler.h"
#include "initial.h"
#include "utils.h"

void processorLocalTimerInt(state_t* processorState) {
    // Acknoledge interrupt e carica il nuovo valore
    setTIMER(TIMESLICE);

    // Salva lo stato attuale di esecuzione del processo
    updateProcessState(processorState, currentProcess);

    // Inserisce il processo nella Ready Queue
    insertProcQ(&readyQueue, currentProcess);

    currentProcess = NULL;

    scheduler();
}

void nonTimerInterrupts(unsigned int excCode, state_t* processorState) {
    if (currentProcess) {
        cpu_t actTime;
        STCK(actTime);
        currentProcess->p_time += actTime - startRunningTime;
    }

    //STCK(startRunningTime);
    unsigned int IntlineNo = excCode - 14;
    
    // 1. Trova il bit del dispositivo (Priorità: bit più basso)
    unsigned int *bitmapAddr = (unsigned int *)(0x10000040 + (IntlineNo - 3) * 0x04);

    int devNo;
    for (devNo = 0; devNo < 8 && !(*bitmapAddr & (1 << devNo)); devNo++);
    
    if (devNo == 8) return;

    // 2. Calcola l'indirizzo base e prepara le variabili
    memaddr devAddrBase = START_DEVREG + ((IntlineNo - 3) * 0x80) + (devNo * 0x10);
    int semIndex = (IntlineNo - 3) * 8 + devNo + 1;
    unsigned int status;

    if (excCode == IL_TERMINAL) { 
        termreg_t *termReg = (termreg_t *)devAddrBase;
        
        if (termReg->transm_status != UNINSTALLED && termReg->transm_status != READY && termReg->transm_status != BUSY) {
            status = termReg->transm_status;
            termReg->transm_command = ACK; 
        } else {
            status = termReg->recv_status;
            termReg->recv_command = ACK;  
            semIndex += 8;              
        }
    } else { 
        dtpreg_t *devReg = (dtpreg_t *)devAddrBase;
        status = devReg->status;
        devReg->command = ACK;            
    }

    pcb_t* readyProc = vOnSem(&deviceSemaphore[semIndex]);
    if (readyProc) {
        readyProc->p_s.reg_a0 = status;
        softBlockCount--;  // era bloccato su device
    }
    
    if (currentProcess) {
        STCK(startRunningTime);
        LDST(processorState);
    }
    else {
        scheduler();
    }
}

void intervalTimer(state_t* processorState) {
    // Ricarica l'interval timer
    LDIT(PSECOND);

    if (currentProcess) {
        cpu_t actTime;
        STCK(actTime);
        currentProcess->p_time += actTime - startRunningTime;
    }

    // Sblocca tutti i processi in attesa del pseudo-clock (V fino a 0)
    while (deviceSemaphore[PSEUDO_SEMAPHORE_INDEX] < 0) {
        vOnSem(&deviceSemaphore[PSEUDO_SEMAPHORE_INDEX]);
        softBlockCount--;
    }
    if (currentProcess) {
        STCK(startRunningTime);
        LDST(processorState);
    }
    else
        scheduler();
}

void deviceInterruptHandler() {
    unsigned int excCode = GET_EXEC_CODE(getCAUSE());

    state_t* processorState = GET_EXCEPTION_STATE_PTR(PROCESSOR_ID);
    switch (excCode)
    {
        case IL_CPUTIMER:
            processorLocalTimerInt(processorState);
            break;
        case IL_TIMER:
            intervalTimer(processorState);
            break;
        case IL_DISK:
        case IL_FLASH:
        case IL_ETHERNET:
        case IL_PRINTER:
        case IL_TERMINAL:
            nonTimerInterrupts(excCode, processorState);
            break;
        default:
            break;
    }        
};