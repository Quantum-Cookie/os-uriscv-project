#include "initial.h"

#include "pcb.h"
#include "asl.h"
#include "exceptions.h"
#include "scheduler.h"

extern void test();

// Variabili globali
int processCount;
int softBlockCount;
struct list_head readyQueue;
pcb_t* currentProcess;
int deviceSemaphore[NRSEMAPHORES];

pcb_t* rootProcess;
cpu_t startRunningTime;


/**
 * @brief Popolazione del Processor 0 Pass Up Vector
 * Imposta i handler, stack pointer per TLB-Refill e le Eccezioni
 * 
 */
static void initPassupvector() {
    passupvector_t* passupvector = (passupvector_t *)PASSUPVECTOR;

    // Indirizzi per gestire eventi TLB-Refill
    passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    passupvector->tlb_refill_stackPtr = KERNELSTACK;

    // Indirizzi per gestire Eccezioni
    passupvector->exception_handler = (memaddr)exceptionHandler;
    passupvector->exception_stackPtr = KERNELSTACK;
}


/**
 * @brief Inizializzazione delle variabili globali e configura Interval Timer
 * 
 */
static void initNucleusData() {
    // Inizializzazione strutture dati di Livello 2
    initPcbs();
    initASL();

    // Inizializzazione variabili globali
    processCount = 0;
    softBlockCount = 0;
    mkEmptyProcQ(&readyQueue);
    currentProcess = NULL;
    for(int i = 0; i < NRSEMAPHORES; i++) deviceSemaphore[i] = 0;

    // Carica 100ms nel Interval Timer 
    LDIT(PSECOND);
}

/**
 * @brief Inizializza il primo processo
 * 
 */
static void initFirstProcess(memaddr process) {
    processCount++;

    // allocPcb imposta gia' i campi con 0/NULL
    pcb_t* firstProcess = allocPcb();

    // Imposta il SP all'ultimo frame della RAM
    RAMTOP(firstProcess->p_s.reg_sp);

    // Abilita gli interrupt e Kernel-mode
    firstProcess->p_s.mie = MIE_ALL;
    firstProcess->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;

    // Imposta il PC del primo processo all'indirizzo passato
    firstProcess->p_s.pc_epc = process;

    // Memorizza come il processo root
    rootProcess = firstProcess;

    // Inserisce il PCB nella readyQueue
    insertProcQ(&readyQueue, firstProcess);
}

// Attualmente il primo processo e' la funzione test di p2test
int main() {
    initPassupvector();
    initNucleusData();
    initFirstProcess((memaddr) test);
    scheduler();
    return 0;
}
