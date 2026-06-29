#include "initProc.h"
#include "vmSupport.h"
#include "const.h"
#include "vmSupport.h"
#include "sysSupport.h"

#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>
#include <uriscv/aout.h>

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

unsigned int uprocHeader[(PAGESIZE / sizeof(unsigned int))];


/**
 * @brief Funzione per inizializzare i semafori mutua esclusione dei dispositivi
 */
static void initSuppSemaphores() {
    for (int i = 0; i < NSUPPSEM; i++) suppIOMutexSemaphores[i] = 1;
}

static void initPageTable(pteEntry_t* pageTable, int asid, unsigned int textPages) {
    for (int i = 0; i < USERPGTBLSIZE; i++) {
        unsigned int vpn;
        if (i < 31) {
            vpn = 0x80000 + i;
            pageTable[i].pte_entryHI = vpn << VPNSHIFT;
        } else {
            vpn = 0xBFFFF;
            pageTable[i].pte_entryHI = vpn << VPNSHIFT;
        }

        pageTable[i].pte_entryHI |= asid << ASIDSHIFT;

        // Le prime 'textPages' sono il testo (.text).
        // La pagina 31 (i == 31) è lo stack e deve essere sempre scrivibile (D=1).
        if (i < textPages && i < 31) {
            // Pagina .text: read-only (D=0)
            pageTable[i].pte_entryLO = 0; 
        } else {
            // Pagina .data o stack: read-write (D=1)
            pageTable[i].pte_entryLO = DIRTYON;
        }
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

    unsigned int semIndex = GET_IO_MUTEX_SEMAPHORE_INDEX(IL_FLASH, asid - 1, 0);

    SYSCALL(PASSEREN, (int)&(suppIOMutexSemaphores[semIndex]), 0, 0);

    // Leggi la pagina 0 del flash (contiene l'header .aout)
    dtpreg_t *flash = (dtpreg_t *) DEV_REG_ADDR(IL_FLASH, asid - 1);
    flash->data0 = (memaddr) uprocHeader;
    int status = SYSCALL(DOIO, (int)&(flash->command), (0 << 8) | FLASHREAD, 0);

    SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semIndex]), 0, 0);

    if (status != READY) {
        // Forza comunque una configurazione standard se la flash fallisce, 
        // giusto per vedere se è questo a far crashare tutto
        initPageTable(supportStructure->sup_privatePgTbl, asid, 0); 
        return;
    }

    // Calcola il numero di pagine .text
    //unsigned int textVaddr = header[AOUT_HE_TEXT_VADDR];
    unsigned int textMemsz = uprocHeader[AOUT_HE_TEXT_MEMSZ];
    //unsigned int textStartPage = (textVaddr >> VPNSHIFT) & 0x1FFFF; // VPN della prima pagina .text
    unsigned int textPages = (textMemsz + PAGESIZE - 1) / PAGESIZE;
    //unsigned int textEndPage = textStartPage + textPages; // VPN escluso della prima pagina .data

    // Passa il numero di pagine di testo
    initPageTable(supportStructure->sup_privatePgTbl, asid, textPages);
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

