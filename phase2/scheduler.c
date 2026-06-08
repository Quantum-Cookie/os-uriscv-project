#include "scheduler.h"

#include <uriscv/liburiscv.h>

#include "initial.h"
#include "pcb.h"
#include "asl.h"

// Scheduler preemptive round-robin basato sulla priorita' dei processi.
void scheduler() {
    // Estrae processo da eseguire nella Ready Queue
    pcb_t* processToRun = removeProcQ(&readyQueue);

    // Controlla se c'e' almeno un processo pronto per l'esecuzione
    if (processToRun) {
        currentProcess = processToRun;

        // Memorizza il tempo in cui e' iniziato l'esecuzione
        STCK(startRunningTime);

        // Imposta 5ms il PLT, ovvero il time slice dedicato
        setTIMER(TIMESLICE * (*((cpu_t *)TIMESCALEADDR)));

        // Ripristina lo stato del processore per il processo
        LDST(&processToRun->p_s);
    }
    else {
        // Non ci sono piu' processi, considerato buon lavoro
        if (processCount == 0)
            HALT();
        // I processi sono in attesa di operazioni I/O
        else if (processCount > 0 && softBlockCount > 0) {
            // Abilita gli interrupt ma disabilita quello del PLT evitando di risvegliarsi per nulla
            setMIE(MIE_ALL & ~MIE_MTIE_MASK);
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK;
            setSTATUS(status);

            // Si mette in stato di attesa fino al prossimo interrupt non PLT
            WAIT();
        }
        // Deadlock: processi presenti ma nessuno è pronto o in attesa di I/O
        else if (processCount > 0 && softBlockCount == 0) {
            PANIC();
        }
    }
}