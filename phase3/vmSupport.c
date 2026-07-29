#include "vmSupport.h"
#include "initProc.h"
#include "sysSupport.h"

#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include <uriscv/arch.h>

// Estrae il codice dell'eccezione
#define GET_EXEC_CODE(cause) (((cause) & CAUSE_EXCCODE_MASK))

// Dimensione della Swap Pool
#define SWAP_POOL_SIZE (2 * UPROCMAX)

// Swap Pool Table: contiene le informazioni riguardanti la pagina logica che occupa una cella (frame).
static swap_t swapPoolTable[SWAP_POOL_SIZE];

// Swap Pool Semaphore: serve per garantire la mutua esclusione nell'accesso della Swap Pool Table
static int swapPoolSemaphore;


/**
 * @brief Marca i frame del Uproc nella Swap Pool con ASID passato come liberi e ne pulisce i campi relativi
 * 
 * @param asid ASID del Uproc di cui si deve liberare i relativi frame nella Swap Pool
 */
void releaseFrames(int asid) {
    // Ottieni semaforo per accedere alla Swap Pool Table in mutua esclusione
    SYSCALL(PASSEREN, (int)&swapPoolSemaphore, 0, 0);

    // Cerca nella Swap Pool Table i frame associati ad tale ASID
    for (int i = 0; i < SWAP_POOL_SIZE; i++) {
        if (swapPoolTable[i].sw_asid == asid) {
            /* Disattiva gli interrupt per aggiornamento atomico della Swap Pool Table e del TLB */
            unsigned int old_status = getSTATUS();
            setSTATUS(old_status & ~MSTATUS_MIE_MASK); 
            
            // Cerca e invalida la entry nella TLB
            setENTRYHI(swapPoolTable[i].sw_pte->pte_entryHI);
            TLBP();
            // Entry trovata: invalida settando V=0
            if ((getINDEX() & PRESENTFLAG) == 0) {
                setENTRYLO(0);
                TLBWI();
            }

            // Marca il frame come libero nella Swap Pool Table
            swapPoolTable[i].sw_asid = NOPROC;
            swapPoolTable[i].sw_pageNo = 0;
            swapPoolTable[i].sw_pte = NULL;

            // Riablilita gli interrupt
            setSTATUS(old_status);
        }
    }

    // Rilascio semaforo per accedere alla Swap Pool Table in mutua esclusione
    SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0, 0);
}

/**
 * @brief Algoritmo di rimpiazzamento delle pagine (Page Replacement). 
 * Cerca prima se ci sono frame liberi nella Swap Pool Table. Se tutti i 
 * frame sono occupati, seleziona una pagina vittima usando una strategia Round Robin.
 * 
 * @note Deve essere chiamata all'interno di una sezione critica protetta da swapPoolSemaphore.
 * @return int Indice del frame selezionato nella Swap Pool Table (valore tra 0 e SWAP_POOL_SIZE-1)
 */
static int replacementAlgorithm() {
    // Variabile che tiene traccia di quale sara' la prossima pagina vittima se non ci sono frame liberi
    static int next_frame = 0;

    // Cerca se esiste un frame libero
    for (int i = 0; i < SWAP_POOL_SIZE; i++) {
        if (swapPoolTable[i].sw_asid == NOPROC) {
            return i;
        }
    }

    // Se non c'e' nessun frame libero, usa round-robin sull'intero pool
    int victim_frame = next_frame;
    next_frame = (next_frame + 1) % SWAP_POOL_SIZE;
    return victim_frame;
}

/**
 * @brief Inizializza le strutture dati dello Swap Pool
 * Imposta il semaforo dello Swap Pool per garantire la mutua esclusione e configura
 * ogni elemento (frame fisico) della Swap Pool Table come libera, azzerando le associazioni
 * con le pagine logiche dei processi utente (U-proc)
 */
void initSwapStructs() {
    // Inizializza il semaforo di mutua esclusione dello Swap Pool a 1
    swapPoolSemaphore = 1;
    
    for (int i = 0; i < SWAP_POOL_SIZE; i++) {
        // Un ASID pari a NOPROC (-1) indica che il frame nello Swap Pool è libero 
        swapPoolTable[i].sw_asid = NOPROC;
        
        // Inizializza il Virtual Page Number (VPN) a 0 in quanto il frame non ospita pagine
        swapPoolTable[i].sw_pageNo = 0;
        
        // Imposta a NULL il puntatore alla Page Table Entry (PTE) 
        swapPoolTable[i].sw_pte = NULL;
    }
}

/**
 * @brief Pager: si occupa di caricare nella Swap Pool la pagina richiesta
 * 
 * La funzione viene invocata quando si verifica un'eccezione di tipo TLB-Invalid
 * (sia su operazione di lettura TLBL, sia di scrittura TLBS). Il suo compito è caricare
 * in RAM (nello Swap Pool) la pagina logica mancante recuperandola dal dispositivo Flash
 * associato all'U-proc corrente.
 */
void TLBPagerHandler() {
    // Ottiene il puntatore al Support Structure del processo corrente
    support_t *sPtr = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    unsigned int cause = sPtr->sup_exceptState[PGFAULTEXCEPT].cause;

    // Se era avvenuto un TLB-Modification exception
    if (GET_EXEC_CODE(cause) == EXC_MOD) {
        programTrapHandler();
    }
    // Altrimenti e' avvenuto un page fault su load/store operation
    else {
        // Mutua esclusione nell'accesso della Swap Pool Table
        SYSCALL(PASSEREN, (int)&swapPoolSemaphore, 0, 0);

        // Determina il numero di pagina (VPN) che ha causato il miss
        unsigned int vpnMissed = ENTRYHI_GET_VPN(sPtr->sup_exceptState[PGFAULTEXCEPT].entry_hi);

        // Se vpnMissed è lo stack (0x3FFFF) l'indice è 31, altrimenti l'indice e' gia' giusto
        vpnMissed = vpnMissed == 0x3FFFF? 31 : vpnMissed;

        unsigned int victimFrame = replacementAlgorithm();

        // Memorizza il vecchio status
        unsigned int old_status;
        old_status = getSTATUS();

        // Verifica se il frame era gia' occupata 
        if (swapPoolTable[victimFrame].sw_asid != NOPROC) {

            /* Aggiornamento atomico del TLB */
            setSTATUS(old_status & ~MSTATUS_MIE_MASK); 

            // Il puntatore alla entry della Page Table del processo vittima
            pteEntry_t* victimPte = swapPoolTable[victimFrame].sw_pte;

            // Invalida tale pagina
            victimPte->pte_entryLO &= ~VALIDON;

            // Configura EntryHi con la pagina logica (VPN) e l'ASID da cercare
            setENTRYHI(victimPte->pte_entryHI);

            // Lancia il Probe hardware nella TLB
            TLBP();

            // Verifica il registro INDEX
            // Se la pagina NON e' in cache, il bit 'P' del registro Index viene impostato a 1
            if ((getINDEX() & PRESENTFLAG) == 0) {
                // La pagina nella TLB

                // Aggiornamento TLB invalidando tale pagina
                setENTRYLO(victimPte->pte_entryLO);
                // Sovrascrive la riga indicata dal registro Index 
                TLBWI(); 
            }

            // Ripristina lo stato (Interrupt di nuovo attivi per l'I/O)
            setSTATUS(old_status);
        }


        // Se il frame era occupato e il Dirty Bit era ON allora lo salva nel flash rispettivo
        if (swapPoolTable[victimFrame].sw_asid != NOPROC && (swapPoolTable[victimFrame].sw_pte->pte_entryLO & DIRTYON) != 0) {

            /* Ottieni semaforo per il dispositivo Flash su cui bisogna memorizzare la pagina */
            unsigned int victimSemIndex = GET_IO_MUTEX_SEMAPHORE_INDEX(IL_FLASH, swapPoolTable[victimFrame].sw_asid - 1, 0);

            SYSCALL(PASSEREN, (int)&(suppIOMutexSemaphores[victimSemIndex]), 0, 0);

            /* Scrittura nel flash nel blocco giusto (VPN) */
            dtpreg_t *victim_flash = (dtpreg_t *) DEV_REG_ADDR(IL_FLASH, swapPoolTable[victimFrame].sw_asid - 1);
            victim_flash->data0 = SWAP_POOL_START_ADDR + (victimFrame * PAGESIZE);
            unsigned int write_cmd = swapPoolTable[victimFrame].sw_pageNo << 8 | FLASHWRITE;

            int wstatus = SYSCALL(DOIO, (unsigned int)&(victim_flash->command), write_cmd, 0);
            
            // Rilascio semaforo per il dispositivo Flash
            SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[victimSemIndex]), 0, 0);

            // Se l'operazione di scrittura era fallita invoca Program Trap
            if (wstatus != READY) {
                SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0, 0);
                programTrapHandler();
            }
        }
        

        /* Ottieni semaforo per il dispositivo Flash da cui bisogna caricare la pagina */
        unsigned int readSemIndex = GET_IO_MUTEX_SEMAPHORE_INDEX(IL_FLASH, sPtr->sup_asid - 1, 0);

        SYSCALL(PASSEREN, (int)&(suppIOMutexSemaphores[readSemIndex]), 0, 0);
        
        // Carica la pagina dal flash relativo al ASID
        dtpreg_t *flash_reg = (dtpreg_t *) DEV_REG_ADDR(IL_FLASH, sPtr->sup_asid - 1);
        flash_reg->data0 = SWAP_POOL_START_ADDR + (victimFrame * PAGESIZE);
        
        unsigned int command = vpnMissed << 8 | FLASHREAD;
        int status = SYSCALL(DOIO, (unsigned int)&(flash_reg->command), command, 0);

        SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[readSemIndex]), 0, 0);

        // Se l'operazione di lettura era fallita invoca Program Trap
        if (status != READY) {
            SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0, 0);
            programTrapHandler();
        }

        /* Disabilita interrupt per aggiornare la Swap Pool Table in modo atomico */  
        setSTATUS(old_status & ~MSTATUS_MIE_MASK); 

        // Aggiorna Swap Pool Table
        swapPoolTable[victimFrame].sw_asid = sPtr->sup_asid;
        swapPoolTable[victimFrame].sw_pageNo = vpnMissed;
        swapPoolTable[victimFrame].sw_pte = &(sPtr->sup_privatePgTbl[vpnMissed]);

        /* Aggiorna la Page Table del processo corrente indicando che la pagina richiesta ora e' valida e che occupa il frame calcolato */
        // Physical Frame Number (PFN) in cui e' stata caricata la pagina
        // La dimensione di un frame e' 4096 (0x1000), quindi i 12 LSB rappresentano l'offset
        // Per isolare il PFN e rimuovere l'offset, si effettua uno shift di 12 bit a destra
        unsigned int pfn = (SWAP_POOL_START_ADDR + (victimFrame * PAGESIZE)) >> 12;
        // Salva lo stato del bit Dirty originale (0 se testo, DIRTYON se dati/stack)
        unsigned int original_dirty = (sPtr->sup_privatePgTbl[vpnMissed].pte_entryLO & DIRTYON);
            
        sPtr->sup_privatePgTbl[vpnMissed].pte_entryLO = (pfn << ENTRYLO_PFN_BIT) | VALIDON | original_dirty;

        // Imposta registro EntryHi per la ricerca nel TLB
        setENTRYHI(sPtr->sup_privatePgTbl[vpnMissed].pte_entryHI);

        // Cerca se la entry è già nella TLB
        TLBP();

        // Imposta registro EntryLo per la scrittura
        setENTRYLO(sPtr->sup_privatePgTbl[vpnMissed].pte_entryLO);

        // Se trovata nel TLB: sovrascrive esattamente quella riga
        if ((getINDEX() & PRESENTFLAG) == 0) {
            TLBWI();
        } 
        // Se non trovata nel TLB: inserisce in una riga casuale
        else {
            TLBWR();
        }

        // Ripristino degli interrupt
        setSTATUS(old_status);

        // Rilascio semaforo per accesso Swap Pool Table
        SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0, 0);

        // Ripristina lo stato del processore
        LDST(&(sPtr->sup_exceptState[PGFAULTEXCEPT]));
    }
}