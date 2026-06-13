#include "utils.h"

#include "initial.h"
#include "pcb.h"
#include "asl.h"

// Funzione ausiliaria per copiare struttura dati state_t
void copyState(state_t* src, state_t* dest) {
    dest->entry_hi = src->entry_hi;
    dest->cause = src->cause;
    dest->status = src->status;
    dest->pc_epc = src->pc_epc;
    dest->mie = src->mie;

    for (int i = 0; i < STATE_GPR_LEN; i++) {
        dest->gpr[i] = src->gpr[i];
    }
}

// Salva lo stato del processore viene salvato nell'apposito campo (p_s)
// e aggiona il tempo di utilizzo CPU accumulato del processo specificato
void updateProcessState(state_t* processorState, pcb_t* process) {
    cpu_t actTime;
    STCK(actTime);

    // Salva lo stato aggiornato del processo 
    copyState(processorState, &process->p_s);

    // Aggiorna tempo totale esecuzione
    process->p_time += actTime - startRunningTime;
}

// Funzione ausiliare per fare operazione di V su un semaforo
pcb_t* vOnSem(int* semAddr) {
    // Controlla se c'e' almeno un processo bloccato 
    pcb_t* readyProc = removeBlocked(semAddr);
    if (readyProc) {
        insertProcQ(&readyQueue, readyProc);
        return readyProc;
    }
    // Nessun processo bloccato, incrementa il valore del semaforo
    (*semAddr)++;
    return NULL;
}