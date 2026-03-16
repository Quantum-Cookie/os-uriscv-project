#include "./headers/exceptions.h"
#include <uriscv/liburiscv.h>
#include "./headers/scheduler.h"

void exceptionHandler() {
    unsigned int cause = getCAUSE();
    
    // Verifica se l'eccezione sia un interrupt
    if (CAUSE_IS_INT(cause)) {
        deviceInterruptHandler();
    }
    else {
        switch (cause) {
            case 24 ... 28:
                tlbExceptionHandler();
                break;

            case 8:
            case 11:
                state_t* processorState = GET_EXCEPTION_STATE_PTR(PROCESSOR_ID);
                syscallExceptionHandler(processorState);
                break;

            case 0 ... 7:
            case 9 ... 10:
            case 12 ... 23:
                programTrapExceptionHandler();
                break;

            default:
                break;
        } 
    }
}

void syscallExceptionHandler(state_t* processorState) {
    switch (processorState->reg_a0) {
        case CREATEPROCESS:
            createProcess(processorState);
            break;
        case TERMPROCESS:
            terminateProcess(processorState);
            break;
    }
}

void copyState(state_t* src, state_t* dest) {
    dest->entry_hi = src->entry_hi;
    dest->cause = src->cause;
    dest->status = src->status;
    dest->pc_epc = src->pc_epc;
    dest->mie = src->mie;

    // Copia i registri generici (GPR)
    for (int i = 0; i < STATE_GPR_LEN; i++) {
        dest->gpr[i] = src->gpr[i];
    }
}

void createProcess(state_t* processorState) {
    pcb_t* newPcb = allocPcb();

    // Controllo se ci sono ancora PCB liberi
    if (!newPcb) {
        processorState->reg_a0 = -1;
        return;
    }

    // p_time, p_semAdd, p_pid gia' inizializzati con allocPcb 
    //newPcb->p_s = *((state_t*)(processorState->reg_a1));
    copyState((state_t*)(processorState->reg_a1), &newPcb->p_s);

    if (processorState->reg_a3)
        newPcb->p_supportStruct = (support_t *)processorState->reg_a3;
    else
        newPcb->p_supportStruct = NULL;

    insertProcQ(&readyQueue, newPcb);
    insertChild(currentProcess, newPcb);
    processCount++;

    // Restituzione pid al chiamante
    processorState->reg_a0 = newPcb->p_pid;
}


/***
 * TODO: Finire terminateProcess
 * 
 * Da decidere come implementare ricerca tramite p_pid:
 * 1. Mantenere lista di tutti i PCB allocati
 * 2. Scansionare a partire dal processo root
 */
void recursiveTermination(pcb_t* toTerminate) {
    // Se il processo non ha figli
    if (emptyChild(toTerminate)) {
        // Se il processo non era nella Ready Queue lo toglie dai processi bloccati nei semafori
        if (!outProcQ(&readyQueue, toTerminate))
            outBlocked(toTerminate);
        freePcb(toTerminate);
    }
    // Se il processo ha figli
    else {
        while (emptyChild(toTerminate))
        {
            // Itera per terminare tutti i processi figli
            pcb_t* childToTerminate = removeChild(toTerminate);
            recursiveTermination(childToTerminate);
        }
    }
}

void terminateProcess(state_t* processorState) {
    if (processorState->reg_a1 == 0) {
        recursiveTermination(currentProcess);
    } 
    else {
        recursiveTermination(currentProcess);
    }
    scheduler();
}

/***
 * FINE TODO
 */

void deviceInterruptHandler() {return;};
void tlbExceptionHandler() {return;};
void programTrapExceptionHandler() {return;};