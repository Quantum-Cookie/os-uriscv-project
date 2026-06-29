#include "initProc.h"
#include "vmSupport.h"
#include "const.h"
#include "vmSupport.h"
#include "sysSupport.h"
#include <uriscv/liburiscv.h>

/* Variabili globali */
// Array unico per tutti i semafori di mutua esclusione dei dispositivi
int suppIOMutexSemaphores[NSUPPSEM];

// Semaforo per evitare che InstantiatorProcess termini prima che termini la shell, garantendo che il sistema rimanga attivo
int masterSemaphore = 0;

// Semaforo per fare si che la shell attenda la terminazione dei processi figli
int shellSemaphore = 0;


/* Variabili locali */ 

// Array per mantenere tutti i Support Structure disponibili
static support_t supportStructures[8];


/**
 * @brief Funzione per inizializzare i semafori mutua esclusione dei dispositivi
 */
static void initSuppSemaphores() {
    for (int i = 0; i < NSUPPSEM; i++) suppIOMutexSemaphores[i] = 1;
}

static void initPageTable(pteEntry_t* pageTable, int asid) {
    for (int i = 0; i < USERPGTBLSIZE; i++) {
        
        // 1. Configurazione del VPN (Virtual Page Number)
        if (i < 31) {
            // Le prime 31 voci vanno da 0x80000 a 0x8001E
            pageTable[i].pte_entryHI = (0x80000 + i) << VPNSHIFT;
        } else {
            // L'ultima voce (31) è la pagina dello stack: 0xBFFFF
            pageTable[i].pte_entryHI = 0xBFFFF << VPNSHIFT;
        }
        
        // 2. Inserimento dell'ASID nel registro EntryHI
        pageTable[i].pte_entryHI |= asid << ASIDSHIFT;
        
        // 3. Configurazione dei bit di controllo in EntryLO (o pte_entryLO)
        // D (Dirty/Write-enabled) = 1 (on)
        pageTable[i].pte_entryLO = DIRTYON;
        
        // G (Global) = 0 (off) -> Non aggiungiamo nessuna maschera globale
        // V (Valid) = 0 (off)  -> Non aggiungiamo la maschera valid (causerà Page Fault all'inizio)
    }
}

static void initSupportStructure(support_t* supportStructure, int asid) {
    supportStructure->sup_asid = asid;

    // 0 - PGFAULTEXCEPT
    supportStructure->sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)TLBPagerHandler;
    supportStructure->sup_exceptContext[GENERALEXCEPT].pc = (memaddr)generalSupportHandler;

    supportStructure->sup_exceptContext[PGFAULTEXCEPT].status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;
    supportStructure->sup_exceptContext[GENERALEXCEPT].status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;

    supportStructure->sup_exceptContext[PGFAULTEXCEPT].stackPtr = (memaddr)&(supportStructure->sup_stackTLB[499]);
    supportStructure->sup_exceptContext[GENERALEXCEPT].stackPtr = (memaddr)&(supportStructure->sup_stackGen[499]);

    initPageTable(supportStructure->sup_privatePgTbl, asid);
}

support_t* allocateSupportStructure(int asid) {    
    initSupportStructure(&supportStructures[asid - 1], asid);
    return &supportStructures[asid - 1];
}

static void initShell() {
    state_t shellState;
    
    // 1. Inizializzazione pulita della struttura (senza sporcizia della RAM)
    unsigned int *ptr = (unsigned int *)&shellState;
    for (int i = 0; i < (sizeof(state_t) / sizeof(unsigned int)); i++) {
        ptr[i] = 0;
    }

    // configura state_t per shell
    shellState.reg_sp = USERSTACKTOP;
    shellState.pc_epc = (memaddr)UPROCSTARTADDR;
    shellState.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_U;
    shellState.mie = MIE_ALL;
    shellState.entry_hi = SHELL_ASID << ASIDSHIFT;

    support_t *shellSupport = allocateSupportStructure(SHELL_ASID);

    int shellPid = SYSCALL(CREATEPROCESS, (int)&shellState, PROCESS_PRIO_LOW, (int)shellSupport);
}

void test() {
    initSwapStructs();
    initSuppSemaphores();

    initShell();

    SYSCALL(PASSEREN, (unsigned int)&masterSemaphore, 0, 0);
}

