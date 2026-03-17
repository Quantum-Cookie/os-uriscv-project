#include "./headers/scheduler.h"
#include "./headers/initial.h"
#include "../phase1/headers/pcb.h"
#include "../phase1/headers/asl.h"
#include <uriscv/liburiscv.h>

void scheduler() {
    // Estrae processo da eseguire nella Ready Queue
    pcb_t* runningProcess = removeProcQ(&readyQueue);

    if (runningProcess) {
        currentProcess = runningProcess;

        // Imposta 5ms il PLT, ovvero il time slice dedicato
        setTIMER(TIMESLICE);

        // Memorizza il tempo in cui e' iniziato l'esecuzione
        STCK(startRunningTime);

        // Ripristina lo stato del processore per il processo
        LDST(&runningProcess->p_s);
    }
    else {
        // Non ci sono piu' processi
        if (processCount == 0)
            HALT();
        // I processi sono in attesa di operazioni I/O
        else if (processCount > 0 && softBlockCount > 0) {
            // Disabilita l'interrupt del PLT evitando di risvegliarsi per nulla
            setMIE(MIE_ALL & ~MIE_MTIE_MASK);
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK;
            setSTATUS(status);

            WAIT();
        }
        // Deadlock
        else if (processCount > 0 && softBlockCount == 0) {
            PANIC();
        }
    }
}