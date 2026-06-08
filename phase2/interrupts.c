#include "interrupts.h"

#include <uriscv/liburiscv.h>

#include "pcb.h"
#include "scheduler.h"
#include "initial.h"
#include "utils.h"

void nonTimerInterrupts(unsigned int excCode, state_t* processorState);
void processorLocalTimerInt(state_t* processorState);
void intervalTimer(state_t* processorState);

// Funzione handler generale delle eccezioni
void deviceInterruptHandler(unsigned int cause, state_t* processorState) {

    // Ottieni exception code
    unsigned int excCode = GET_EXEC_CODE(cause);

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
        // Non ci dovrebbe arrivare
        default:
            break;
    }        
};

/**
 * @brief Funzione handler per Processor Local Timer (PLT)
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 */
void processorLocalTimerInt(state_t* processorState) {
    // Acknoledge interrupt e carica il nuovo valore
    setTIMER(TIMESLICE);

    // Aggiorna lo stato processo in esecuzione
    updateProcessState(processorState, currentProcess);

    // Inserisce il processo nella Ready Queue
    insertProcQ(&readyQueue, currentProcess);

    currentProcess = NULL;

    scheduler();
}

/**
 * @brief Funzione handler per gli interrupt non timer (dispositivi I/O)
 * 
 * @param excCode Interrupt exception code
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 */
void nonTimerInterrupts(unsigned int excCode, state_t* processorState) {
    // Se era in esecuzione un processo allora aggiorna il suo tempo di esecuzione prima della gestione effettiva dell'interrupt
    if (currentProcess) {
        cpu_t actTime;
        STCK(actTime);
        currentProcess->p_time += actTime - startRunningTime;
    }

    unsigned int IntlineNo = excCode - 14;
    
    // Inizio del Device Bit Map per la classe di dispositivo nel IntLineNo calcolato
    unsigned int *bitmapAddr = (unsigned int *)(0x10000040 + (IntlineNo - 3) * 0x04);

    // Trova il bit del dispositivo da gestire (Priorità: bit più basso)
    int devNo;
    for (devNo = 0; devNo < 8 && !(*bitmapAddr & (1 << devNo)); devNo++);
    
    // Significa che non ha trovato dispositivo che ha generato interrupt (teoricamente impossibile)
    if (devNo == 8) return;

    // Calcola l'indirizzo base del dispositivo da gestire
    memaddr devAddrBase = START_DEVREG + ((IntlineNo - 3) * 0x80) + (devNo * 0x10);

    // Calcolo indice semaforo in base alla suddivisione (si veda documentazione)
    int semIndex = (IntlineNo - 3) * 8 + devNo + 1;

    // Si ritorna status code per il processo che aveva richiesto l'operazione
    unsigned int status;

    // Se interrupt era stato provocato da un dispositivo Terminal
    if (excCode == IL_TERMINAL) { 
        termreg_t *termReg = (termreg_t *)devAddrBase;
        
        // Controllo se c'era pendente una richiesta dal Terminal Transmitter
        if (termReg->transm_status != UNINSTALLED && termReg->transm_status != READY && termReg->transm_status != BUSY) {
            status = termReg->transm_status;
            termReg->transm_command = ACK; 
        } 
        // Altrimenti era un Terminal Receiver
        else {
            status = termReg->recv_status;
            termReg->recv_command = ACK;  
            // Nell'array dei semafori viene dopo i Terminal Transmitter
            semIndex += 8;              
        }
    } 
    // Altri classi di dispositivi
    else { 
        dtpreg_t *devReg = (dtpreg_t *)devAddrBase;
        status = devReg->status;
        devReg->command = ACK;            
    }

    pcb_t* readyProc = vOnSem(&deviceSemaphore[semIndex]);

    // Se il dispositivo che chiedeva la richiesta e' ancora presente e non terminata durante l'operazione I/O
    if (readyProc) {
        readyProc->p_s.reg_a0 = status;
        softBlockCount--;  // era bloccato su device
    }
    
    // Se c'era processo in esecuzione lo fa partire 
    if (currentProcess) {
        STCK(startRunningTime);
        LDST(processorState);
    }
    // Altrimenti chiama lo scheduler
    else {
        scheduler();
    }
}

/**
 * @brief Funzione handler per Interval Timer (Pseudo-clock)
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 */
void intervalTimer(state_t* processorState) {
    // Ricarica l'interval timer
    LDIT(PSECOND);

    // Se c'era processo in esecuzione non gli accredita tempo gestione di tale interrupt
    if (currentProcess) {
        cpu_t actTime;
        STCK(actTime);
        currentProcess->p_time += actTime - startRunningTime;
    }

    // Sblocca tutti i processi in attesa del pseudo-clock
    // vOnSem restituisce NULL quando non ci sono più processi bloccati (e incrementa il semaforo)
    pcb_t* unblocked;
    do {
        unblocked = vOnSem(&deviceSemaphore[PSEUDO_SEMAPHORE_INDEX]);
        if (unblocked) softBlockCount--;
    } while (unblocked != NULL);

    // L'ultima vOnSem ha incrementato il semaforo a 1 non trovando processi bloccati, lo riportiamo a 0    
    deviceSemaphore[PSEUDO_SEMAPHORE_INDEX] = 0;

    // Se c'era processo in esecuzione quando era scattato interrupt lo fa ripartire
    if (currentProcess) {
        STCK(startRunningTime);
        LDST(processorState);
    }
    // Altrimenti chiama lo scheduler
    else
        scheduler();
}

