#include "initProc.h"
#include "vmSupport.h"
#include "const.h"
#include "vmSupport.h"
#include "sysSupport.h"

#include <uriscv/liburiscv.h>
#include <uriscv/arch.h>
#include <uriscv/aout.h>

/* Variabili globali */
// Array unico per tutti i semafori di mutua esclusione dei dispositivi I/O
int suppIOMutexSemaphores[NSUPPSEM];

// Semaforo per evitare che InstantiatorProcess termini prima che termini la shell, garantendo che il sistema rimanga attivo
int masterSemaphore = 0;

// Semaforo per bloccare la shell in attesa del comando di terminazione del figlio
int shellSemaphore = 0;

// Indirizzo di inizio della Swap Pool (subito dopo area del sistema operativo)
memaddr SWAP_POOL_START_ADDR;


/* Variabili locali */ 
// Array per mantenere tutti i Support Structure destinate ai processi utente (U-proc)
static support_t supportStructures[8];

// Buffer della dimensione di una pagina per leggere l'header aout del U-proc e configurare l'area .text come Read-Only
static unsigned int uprocHeader[(PAGESIZE / sizeof(unsigned int))];

// Semaforo per mutua esclusione su tale variabile
static int uprocHeaderSemaphore = 1;

// Memorizza ulltimo indirizzo della RAM
static memaddr ramTop;

// Testa della lista (free list) per la gestione e il riuso delle Support Structures libere
static struct list_head supportFree_h;

// Indirizzo base da cui allocare, muovendosi verso il basso, gli stack dei gestori TLB e General del Support Level
static memaddr supportStacksBase;



/**
 * @brief Estrae il primo elemento dalla free list (analogo a listRemoveFirst di pcb.c)
 * 
 * @param head Puntatore alla testa della lista degli Support Structure liberi
 * @return struct list_head* Puntatore alla Support Structure libera, NULL se non ce sono
 */
static struct list_head *suppListRemoveFirst(struct list_head *head) {
    // Controllo per vedere se la lista e' vuota o meno
    if (list_empty(head))
        return NULL;
    
    // Rimuove primo elemento della lista e pulisce i suoi campi di list_head
    struct list_head *toRemove = head->next;
    list_del(toRemove);

    return toRemove;
}

/**
 * @brief Inizializza la free list delle Support Structures, inserendo
 * tutti gli elementi dell'array statico
 */
static void initSupportFreeList() {
    INIT_LIST_HEAD(&supportFree_h);

    /* Inizializzazione dei puntatori della lista interna al Support Structure 
    e inserimento del Support Structure corrente nella lista dei Support Structure liberi */
    for (int i = 0; i < 8; i++) {
        INIT_LIST_HEAD(&(supportStructures[i].s_list));
        list_add_tail(&(supportStructures[i].s_list), &supportFree_h);
    }
}

/**
 * @brief Reinserisce la Support Structure alla free list
 * 
 * @note Reinserisce senza pulire i campi
 */
void deallocateSupportStructure(support_t* s) {
    if (!s) return;
    list_add_tail(&(s->s_list), &supportFree_h);
}


/**
 * @brief Funzione per inizializzare i semafori di mutua esclusione dei dispositivi
 * Imposta a 1 (libero) tutti i semafori nell'array suppIOMutexSemaphores
 */
static void initSuppSemaphores() {
    for (int i = 0; i < NSUPPSEM; i++) suppIOMutexSemaphores[i] = 1;
}

/**
 * @brief Inizializza la Page Table del Uproc
 * 
 * @param pageTable Puntatore alla Page Table privata
 * @param asid ASID relativo al Uproc
 * @param textPages Il numero di pagine .text (da mettere in read only)
 */
static void initPageTable(pteEntry_t* pageTable, int asid, unsigned int textPages) {
    for (int i = 0; i < USERPGTBLSIZE; i++) {
        /* Configurazione del EntryHI*/
        unsigned int vpn;
        
        // i primi 31 pagine sono per .text e .data con VPN (Virtual Page Number) vanno da 0x80000 a 0x8001E
        if (i < 31) {
            vpn = 0x80000 + i;
            pageTable[i].pte_entryHI = vpn << VPNSHIFT;
        } 
        // La 32-esima pagina e' per lo stack con VPN 0xBFFFF
        else {
            vpn = 0xBFFFF;
            pageTable[i].pte_entryHI = vpn << VPNSHIFT;
        }

        // Inserimento ASID
        pageTable[i].pte_entryHI |= asid << ASIDSHIFT;

        /* configurazione del EntryLO */

        // All'inizializzazione le pagine non sono valide (Valid bit = 0) e non hanno un frame fisico associato.
        // Impostiamo solo il bit Dirty (D) per determinare se la pagina sarà modificabile o meno:
        // - Pagine .text (i < textPages): Read-Only -> Bit D = 0
        // - Pagine .data o Stack (i == 31): Read-Write -> Bit D = 1 (DIRTYON)
        if (i < textPages && i < 31) {
            // Segmento di testo protetto da scrittura
            pageTable[i].pte_entryLO = 0; 
        } else {
            // Segmento dati o stack modificabile
            pageTable[i].pte_entryLO = DIRTYON;
        }
    }
}


/**
 * @brief Inizializza la base per gli stack del Support Level.
 * L'ultimo frame di RAM (da RAMTOP - PAGESIZE a RAMTOP) è strettamente 
 * riservato allo stack del processo InstantiatorProcess(). Le aree di stack per gli 
 * handler del Support Level vengono posizionate immediatamente al di sotto.
 */
static inline void initSupportStacksBase() {
    // L'ultimo frame (RAMTOP - PAGESIZE .. RAMTOP) è riservato allo stack di InstantiatorProcess().
    // Le aree di stack per i Support Level handler partono subito sotto.
    supportStacksBase = ramTop - PAGESIZE;
}

/**
 * @brief Restituisce il valore iniziale dello Stack Pointer (SP) per il TLB exception handler.
 * Poiché lo stack cresce verso il basso, lo SP deve puntare alla fine (limite superiore)
 * della pagina fisica allocata. Ogni U-proc ha a disposizione 2 frame dedicati; 
 * al TLB handler viene assegnato il primo frame a partire dall'alto.
 * 
 * @param asid ASID del Uproc a cui si sta assegnado lo stack
 * @return memaddr Indirizzo iniziale dello Stack Pointer (fine della pagina)
 */
static inline memaddr tlbStackSP(int asid) {
    // 2 Frame per un Uproc, al TLB exception handler va il primo a partire dall'alto (piu' in alto)
    return supportStacksBase - (2 * (asid - 1)) * PAGESIZE;
}

/**
 * @brief Restituisce il valore iniziale dello Stack Pointer (SP) per il General exception handler.
 * Poiché lo stack cresce verso il basso, lo SP deve puntare alla fine (limite superiore)
 * della pagina fisica allocata. Ogni U-proc ha a disposizione 2 frame dedicati;
 * al General handler viene assegnato il secondo frame a partire dall'alto.
 * 
 * @param asid ASID del Uproc a cui si sta assegnado lo stack
 * @return memaddr Indirizzo iniziale dello Stack Pointer (fine della pagina)
 */
static inline memaddr genStackSP(int asid) {
    // 2 Frame per un Uproc, al General exception handler va il secondo a partire dall'alto (piu' in basso)
    return supportStacksBase - (2 * (asid - 1) + 1) * PAGESIZE;
}

/**
 * @brief Inizializzazione della Support Structure
 * 
 * @param supportStructure Puntatore al Support Structure da inizializzare
 * @param asid ASID del Uproc a cui e' destinato tale Support Structure
 */
static void initSupportStructure(support_t* supportStructure, int asid) {
    supportStructure->sup_asid = asid;

    // Inizializzazione del PC per i vari handler
    // PGFAULTEXCEPT -> Support Level’s TLB handler
    supportStructure->sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)TLBPagerHandler;
    // GENERALEXCEPT -> Support Level’s general exception handler
    supportStructure->sup_exceptContext[GENERALEXCEPT].pc = (memaddr)generalSupportHandler;

    // Status con tutti gli interrupt attivi (handler interrompibili)
    supportStructure->sup_exceptContext[PGFAULTEXCEPT].status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;
    supportStructure->sup_exceptContext[GENERALEXCEPT].status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;

    // Stack pointer dei rispettivi handler
    supportStructure->sup_exceptContext[PGFAULTEXCEPT].stackPtr = tlbStackSP(asid);
    supportStructure->sup_exceptContext[GENERALEXCEPT].stackPtr = genStackSP(asid);

    
    /* Lettura sul flash relativo a tale ASID per ottenere header di tale Uproc */
    unsigned int semIndex = GET_IO_MUTEX_SEMAPHORE_INDEX(IL_FLASH, asid - 1, 0);

    // Acquisizione semafori per Flash e Buffer comune per header
    SYSCALL(PASSEREN, (int)&(suppIOMutexSemaphores[semIndex]), 0, 0);
    SYSCALL(PASSEREN, (int)&(uprocHeaderSemaphore), 0, 0);
    
    // Leggi la pagina 0 del flash (contiene l'header)
    dtpreg_t *flash = (dtpreg_t *) DEV_REG_ADDR(IL_FLASH, asid - 1);
    flash->data0 = (memaddr) uprocHeader;
    int status = SYSCALL(DOIO, (int)&(flash->command), (0 << 8) | FLASHREAD, 0);

    // Se l'operazione di lettura sul Flash fallira', allora impostiamo tutte la pagine come writable
    // Come succedeva quando non c'era tale ottimizzazione, altrimenti si puo' mettere un valore di ritorno
    // per rappresentare l'errore
    unsigned int textPages = 0;

    // Se la lettura sul Flash è riuscita, estraiamo subito il dato dal buffer comune
    if (status == READY) {
        unsigned int textMemsz = uprocHeader[AOUT_HE_TEXT_MEMSZ];
        // Sovrastima per avere il Ceil delle pagine .text
        textPages = (textMemsz + PAGESIZE - 1) / PAGESIZE;
    }

    // Rilascio dei semafori
    SYSCALL(VERHOGEN, (int)&(uprocHeaderSemaphore), 0, 0);
    SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semIndex]), 0, 0);

    // Inizializza la Page Table privata (se status != READY, textPages sarà 0 e tutte le pagine saranno writable)
    initPageTable(supportStructure->sup_privatePgTbl, asid, textPages);
}

/**
 * @brief Allocazione del Support Structure per un certo Uproc
 * 
 * @param asid ASID del Uproc di cui si vuole instanziare Support Structure
 * @return support_t* puntatore al Support Structure, NULL se non ce ne sono liberi
 */
support_t* allocateSupportStructure(int asid) {
    struct list_head *removed = suppListRemoveFirst(&supportFree_h);
    // Nessuna struttura libera disponibile
    if (!removed)
        return NULL; 

    // Estrapola il puntatore al Support Structure
    support_t *s = container_of(removed, support_t, s_list);
    initSupportStructure(s, asid);
    return s;
}

/**
 * @brief Funzione che fa partire l'esecuzione della shell
 * 
 */
static void initShell() {
    state_t shellState;
    
    // configura state_t per shell, non c'e' bisogno di pulire in quanto si vanno ad assegnare 
    // tutti i 5 campi, tranne gpr che verra' gestito in automatico in caso d'eccezione

    // Stack Pointer all'interno del kuseg
    shellState.reg_sp = USERSTACKTOP;
    // PC punta all'inizio del segmento .text
    shellState.pc_epc = (memaddr)UPROCSTARTADDR;
    // Eseguito in user mode con tutti gli interrupt attivi
    shellState.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_U;
    shellState.mie = MIE_ALL;
    
    // Inserimento ASID in EntryHI
    shellState.entry_hi = SHELL_ASID << ASIDSHIFT;

    // Alloca la Support Structure
    support_t *shellSupport = allocateSupportStructure(SHELL_ASID);

    // Se allocazione per il Support Structure fallisce
    if (!shellSupport) {
        // La Shell non può partire
        SYSCALL(TERMPROCESS, 0, 0, 0);
        return;
    }

    // Creazione del processo utente (Shell) tramite il Nucleo
    int shellPid = SYSCALL(CREATEPROCESS, (int)&shellState, PROCESS_PRIO_LOW, (int)shellSupport);
    // Se la creazione fallisce
    if (shellPid == -1) {
        // La Shell non può partire
        SYSCALL(TERMPROCESS, 0, 0, 0);
        return;
    }
}

/**
 * @brief Inizializza la variabile che memorizza inizio della Swap Pool
 * 
 */
void initSwapPoolPosition() {
    /* L'header dell'eseguibile si trova in RAM subito dopo la pagina riservata al BIOS.
       In base a CORE_HDR_SIZE, l'indice corretto per saltare il blocco BIOS e il tag ID 
       e' esattamente RAMSTART + 1024 word (ovvero CORE_HDR_SIZE - 1), in quanto comprende
       anche id tag */
    unsigned int *os_header = (unsigned int *)RAMSTART + CORE_HDR_SIZE - 1;

    /* Estrae l'indirizzo virtuale di inizio della sezione .data e la sua dimensione complessiva.
       Essendo .data l'ultimo segmento dell'OS, delimita il confine superiore del codice del Kernel. */
    unsigned int data_vaddr = os_header[AOUT_HE_DATA_VADDR];
    unsigned int data_memsz = os_header[AOUT_HE_DATA_MEMSZ];

    unsigned int os_end = data_vaddr + data_memsz;

    /* Calcola l'indirizzo di inizio dello Swap Pool eseguendo un allineamento per eccesso (Ceil) 
       al limite della pagina fisica successiva.
       - (os_end + PAGESIZE - 1): Spinge l'indirizzo in avanti nella pagina successiva (se non allineato).
       - & ~(PAGESIZE - 1): Azzera i bit di offset inferiore tramite maschera bit a bit, 
       troncando il valore all'inizio esatto della nuova pagina libera. */
    SWAP_POOL_START_ADDR = (os_end + PAGESIZE - 1) & ~(PAGESIZE - 1);
}

/**
 * @brief Processo iniziatore del Support Level.
 * Configura l'ambiente per l'esecuzione dei processi utente (U-proc):
 * determina i limiti della RAM, calcola la posizione dello Swap Pool, 
 * inizializza i semafori di mutua esclusione per i dispositivi di I/O 
 * e prepara la lista delle Support Structure libere. Infine, fa partire
 * shell si blocca in attesa della sua terminazione 
 */
void InstantiatorProcess() {
    /* Recupera dall'hardware l'indirizzo fisico massimo della RAM installata 
       e lo memorizza nella variabile ramTop. */
    RAMTOP(ramTop); 
    initSupportStacksBase();
    initSwapPoolPosition();
    initSwapStructs();
    initSuppSemaphores();
    initSupportFreeList(); 
    initShell();

    /* Effettua una P sul masterSemaphore per bloccare questo processo.
       Rimarrà bloccato finche' la shell non terminera' */
    SYSCALL(PASSEREN, (unsigned int)&masterSemaphore, 0, 0);
}