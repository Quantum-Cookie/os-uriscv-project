#include "./headers/initial.h"
#include "../phase1/headers/pcb.h"
#include "../phase1/headers/asl.h"
#include "./headers/exceptions.h"
#include "./headers/scheduler.h"

static void initPassupvector();
static void initNucleusData();
static void initFirstProcess();

extern void uTLB_RefillHandler();
extern void test();

// Variabili globali
int processCount;
int softBlockCount;
struct list_head readyQueue;
pcb_t* currentProcess;
int deviceSemaphore[NRSEMAPHORES];

static void initPassupvector() {
    passupvector_t* passupvector = (passupvector_t *)PASSUPVECTOR;

    // Indirizzi per gestire eventi TLB-Refill
    passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    passupvector->tlb_refill_stackPtr = KERNELSTACK;

    // Indirizzi per gestire Eccezioni
    passupvector->exception_handler = (memaddr)exceptionHandler;
    passupvector->exception_stackPtr = KERNELSTACK;
}

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

    // Carica 100ms nel Internal Timer 
    LDIT(PSECOND);
}

static void initFirstProcess() {
    processCount++;

    // allocPcb imposta gia' i campi con 0/NULL
    pcb_t* firstProcess = allocPcb();

    // Imposta il SP all'ultimo frame della RAM
    RAMTOP(firstProcess->p_s.reg_sp);

    // Abilita gli interrupt e Kernel-mode
    firstProcess->p_s.mie = MIE_ALL;
    firstProcess->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;

    // Imposta il PC del primo processo alla funzione test
    firstProcess->p_s.pc_epc = (memaddr) test;

    insertProcQ(&readyQueue, firstProcess);
}

int main() {
    initPassupvector();
    initNucleusData();
    initFirstProcess();
    scheduler();
}