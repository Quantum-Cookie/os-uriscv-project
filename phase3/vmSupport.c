#include "vmSupport.h"
#include "initProc.h"
#include "sysSupport.h"

#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include <uriscv/arch.h>

// Restituisce (evetualmente interrupt) exception code
#define GET_EXEC_CODE(cause) (((cause) & CAUSE_EXCCODE_MASK))

#define SWAP_POOL_SIZE (2 * UPROCMAX)

// Swap Pool Table: contiene informazioni riguardante la pagina logica che occupa un cella 
static swap_t swapPoolTable[SWAP_POOL_SIZE];

// Swap Pool Semaphore: serve per garantire la mutua esclusione nell'accesso della Swap Pool Table
int swapPoolSemaphore;

// Questa variabile tiene traccia di quale frame allocare la prossima volta
static int next_frame = 0;

void releaseFrames(int asid) {
    SYSCALL(PASSEREN, (int)&swapPoolSemaphore, 0, 0);

    unsigned int old_status = getSTATUS();
    setSTATUS(old_status & ~MSTATUS_MIE_MASK); // interrupt OFF

    for (int i = 0; i < SWAP_POOL_SIZE; i++) {
        if (swapPoolTable[i].sw_asid == asid) {
            // Cerca e invalida la entry nella TLB
            setENTRYHI(swapPoolTable[i].sw_pte->pte_entryHI);
            TLBP();
            if ((getINDEX() & PRESENTFLAG) == 0) {
                // Entry trovata: invalida settando V=0
                setENTRYLO(0);
                TLBWI();
            }

            // Marca il frame come libero nella swap pool
            swapPoolTable[i].sw_asid = NOPROC;
            swapPoolTable[i].sw_pageNo = 0;
            swapPoolTable[i].sw_pte = NULL;
        }
    }

    setSTATUS(old_status); // interrupt ON

    SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0, 0);
}

static int replacementAlgorithm() {
    // Prima passa: cerca un frame libero (evita una FLASHWRITE)
    for (int i = 0; i < SWAP_POOL_SIZE; i++) {
        if (swapPoolTable[i].sw_asid == NOPROC) {
            return i;
        }
    }

    // Nessun frame libero: round-robin sull'intero pool
    int frame_da_usare = next_frame;
    next_frame = (next_frame + 1) % SWAP_POOL_SIZE;
    return frame_da_usare;
}

// Funzione per inizializzare Inizializza le strutture dati dello Swap Pool
void initSwapStructs() {
    /* Inizializza il semaforo di mutua esclusione dello Swap Pool a 1 */
    swapPoolSemaphore = 1;
    
    for (int i = 0; i < SWAP_POOL_SIZE; i++) {
        /* Un ASID pari a NOPROC (-1) indica che il frame nello Swap Pool è libero */
        swapPoolTable[i].sw_asid = NOPROC;
        
        /* Inizializza il Virtual Page Number (VPN) a 0 in quanto il frame non ospita pagine */
        swapPoolTable[i].sw_pageNo = 0;
        
        /* Imposta a NULL il puntatore alla Page Table Entry (PTE) */
        swapPoolTable[i].sw_pte = NULL;
    }
}

void TLBPagerHandler() {
    // Ottiene il puntatore al Support Structure del processo corrente
    support_t *sPtr = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    unsigned int cause = sPtr->sup_exceptState[PGFAULTEXCEPT].cause;

    if (GET_EXEC_CODE(cause) == EXC_MOD) {
        programTrapHandler();
    }
    else {
        // Mutua esclusione nell'accesso della Swap Pool Table
        SYSCALL(PASSEREN, (int)&swapPoolSemaphore, 0, 0);

        // Determina il numero di pagina (VPN) che ha causato il miss
        unsigned int vpnMissed = ENTRYHI_GET_VPN(sPtr->sup_exceptState[PGFAULTEXCEPT].entry_hi);

        // Se vpnMissed è lo stack (0x3FFFF) l'indice è 31, altrimenti l'indice e' gia' giusto
        vpnMissed = vpnMissed == 0x3FFFF? 31 : vpnMissed;

        unsigned int victimFrame = replacementAlgorithm();

        unsigned int old_status;
        old_status = getSTATUS();
        setSTATUS(old_status & ~MSTATUS_MIE_MASK); // Disabilita interrupt

        // Verifica se il frame era gia' occupata 
        if (swapPoolTable[victimFrame].sw_asid != NOPROC) {
            // Il puntatore alla entry della Page Table del processo VITTIMA
            pteEntry_t* victimPte = swapPoolTable[victimFrame].sw_pte;

            victimPte->pte_entryLO &= ~VALIDON;

            // Configura EntryHi con la pagina logica (VPN) e l'ASID da cercare
            setENTRYHI(victimPte->pte_entryHI);

            // Lancia il Probe hardware nella TLB
            TLBP();

            // Verifica il registro INDEX.
            // Se la pagina NON è in cache, il bit 'P' (Probe Failure) del registro Index viene impostato a 1.
            // Se la pagina È in cache, il bit 'P' è 0 e il resto del registro contiene l'indice esatto (0, 1, 2...).

            if ((getINDEX() & PRESENTFLAG) == 0) {
                // La pagina ERA IN CACHE (nella TLB)!
                // Index.P è 0, quindi getINDEX() restituisce la posizione hardware della riga. 

                // Aggiornamento TLB
                setENTRYHI(victimPte->pte_entryHI);
                setENTRYLO(victimPte->pte_entryLO);
                TLBWI(); // Sovrascrive la riga indicata dal registro Index 
            }
        }
        
        setSTATUS(old_status); // Ripristina lo stato (Interrupt di nuovo attivi per l'I/O)


        if (swapPoolTable[victimFrame].sw_asid != NOPROC) {
            unsigned int victimSemIndex = GET_IO_MUTEX_SEMAPHORE_INDEX(IL_FLASH, swapPoolTable[victimFrame].sw_asid - 1, 0);

            SYSCALL(PASSEREN, (int)&(suppIOMutexSemaphores[victimSemIndex]), 0, 0);

            dtpreg_t *victim_flash = (dtpreg_t *) DEV_REG_ADDR(IL_FLASH, swapPoolTable[victimFrame].sw_asid - 1);
            victim_flash->data0 = SWAP_POOL_START_ADDR + (victimFrame * PAGESIZE);
            unsigned int write_cmd = swapPoolTable[victimFrame].sw_pageNo << 8 | FLASHWRITE;

            int wstatus = SYSCALL(DOIO, (unsigned int)&(victim_flash->command), write_cmd, 0);
            
            if (wstatus != READY) {
                SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0, 0);
                programTrapHandler();
            }

            SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[victimSemIndex]), 0, 0);
        }
        

        unsigned int readSemIndex = GET_IO_MUTEX_SEMAPHORE_INDEX(IL_FLASH, sPtr->sup_asid - 1, 0);

        SYSCALL(PASSEREN, (int)&(suppIOMutexSemaphores[readSemIndex]), 0, 0);
        
        // Carica la pagina dal flash relativo al ASID
        dtpreg_t *flash_reg = (dtpreg_t *) DEV_REG_ADDR(IL_FLASH, sPtr->sup_asid - 1);
        flash_reg->data0 = SWAP_POOL_START_ADDR + (victimFrame * PAGESIZE);
        
        unsigned int command = vpnMissed << 8 | FLASHREAD;
        int status = SYSCALL(DOIO, (unsigned int)&(flash_reg->command), command, 0);

        SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[readSemIndex]), 0, 0);

        if (status != READY) {
            SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0, 0);
            programTrapHandler();
        }

        setSTATUS(old_status & ~MSTATUS_MIE_MASK); // Disabilita interrupt

        // Aggiorna Swap Pool Table
        swapPoolTable[victimFrame].sw_asid = sPtr->sup_asid;
        swapPoolTable[victimFrame].sw_pageNo = vpnMissed;
        swapPoolTable[victimFrame].sw_pte = &(sPtr->sup_privatePgTbl[vpnMissed]);

        // Update the Current Process’s Page Table entry for page p to indicate it is now present (V bit) and occupying frame i (PFN field).
        unsigned int framePAddr = (SWAP_POOL_START_ADDR + (victimFrame * PAGESIZE)) >> ENTRYLO_PFN_BIT;
        // 2. Salva lo stato del bit Dirty originale (0 se testo, DIRTYON se dati/stack)
        unsigned int original_dirty = (sPtr->sup_privatePgTbl[vpnMissed].pte_entryLO & DIRTYON);
            
        // 3. ASSEGNA pulendo completamente i vecchi dati e unendo i nuovi pezzi
        sPtr->sup_privatePgTbl[vpnMissed].pte_entryLO = (framePAddr << ENTRYLO_PFN_BIT) | VALIDON | original_dirty;

        // Dopo aver aggiornato la Page Table entry del processo corrente...
        setENTRYHI(sPtr->sup_privatePgTbl[vpnMissed].pte_entryHI);
        setENTRYLO(sPtr->sup_privatePgTbl[vpnMissed].pte_entryLO);

        // Cerca se la entry è già nella TLB
        TLBP();

        if ((getINDEX() & PRESENTFLAG) == 0) {
            // Trovata: sovrascrive esattamente quella riga
            TLBWI();
        } else {
            // Non trovata: inserisce in una riga casuale
            TLBWR();
        }

        setSTATUS(old_status);

        SYSCALL(VERHOGEN, (int)&swapPoolSemaphore, 0, 0);

        LDST(&(sPtr->sup_exceptState[PGFAULTEXCEPT]));
    }
}