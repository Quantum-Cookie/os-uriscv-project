#include "utils.h"

#include "initial.h"
#include "pcb.h"
#include "asl.h"

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

/**
 * @brief Salva lo stato del processo attuale nell'apposito campo e aggiona il tempo di utilizzo CPU accumulato
 * 
 * @param processorState Puntatore allo stato del processore prima dell'eccezione
 */
void updateProcessState(state_t* processorState, pcb_t* process) {
    cpu_t actTime;
    STCK(actTime);

    copyState(processorState, &process->p_s);
    process->p_time += actTime - startRunningTime;
}

pcb_t* vOnSem(int* semAddr) {
    (*semAddr)++;
    pcb_t* readyProc = NULL;
    if (*semAddr <= 0) {
        readyProc = removeBlocked(semAddr);
        if (readyProc)
            insertProcQ(&readyQueue, readyProc);

    }
    return readyProc;
}