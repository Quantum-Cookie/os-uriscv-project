#include "sysSupport.h"
#include "initProc.h"
#include "vmSupport.h"
#include "types.h"
#include "const.h"

#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include <uriscv/arch.h>


// Maschera per ottenere status dei device terminal
#define TERMSTATMASK 0xFF

// Estrae il codice dell'eccezione
#define GET_EXEC_CODE(cause) (((cause) & CAUSE_EXCCODE_MASK))

static void syscallHandler();

static void terminate(support_t* sPtr);
static void writeTerminal(support_t* sPtr);
static void readTerminal(support_t* sPtr);
static void execute(support_t* sPtr);

/**
 * @brief Handler che gestisce tutte le eccezioni passati al Support Level non TLB
 * - Tutte le SYSCALL exceptions numberati >= 1
 * - Tutti i Program Trap exceptions
 */
void generalSupportHandler() {
    // Ottiene Support Structure del processo corrente ed estrae exception code
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    unsigned int excCode = GET_EXEC_CODE(sPtr->sup_exceptState[GENERALEXCEPT].cause);
    
    // Gestione delle SYSCALL
    if (excCode == 8 || excCode == 11) {
        syscallHandler();
    } 
    // Gestione dei Program Trap
    else {
        programTrapHandler();
    }
}

/**
 * @brief Handler delle SYSCALL
 * Smista in base al valore messo a0 l'azione giusta
 * 
 */
void syscallHandler() {
    // Ottiene puntatore Support Structure del processo che ha generato l'eccezione ed estrae exception code
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    // Incrementa PC per evitare loop sulla chiamata di SYSCALL
    sPtr->sup_exceptState[GENERALEXCEPT].pc_epc += 4;

    switch (sPtr->sup_exceptState[GENERALEXCEPT].reg_a0)
    {
        case TERMINATE:
            terminate(sPtr);
            break;
        case WRITETERMINAL:
            writeTerminal(sPtr);
            break;
        case READTERMINAL:
            readTerminal(sPtr);
            break;
        case EXECUTE:
            execute(sPtr);
            break;
        default:
            break;
    }

    // Ripristina lo stato del processore
    LDST(&(sPtr->sup_exceptState[GENERALEXCEPT]));
}

/**
 * @brief SYS2: Terminate
 * Termina l'esecuzione del Uproc che l'ha invocato. Se e' stato la Shell effettua V su 
 * masterSemaphore per far terminare il sistema; altrimenti si fa una V
 * su shellSemaphore per far riprendere l'esecuzione al processo Shell
 * 
 * @param sPtr Puntatore Support Structure del processo corrente che ha generato l'eccezione
 */
static void terminate(support_t* sPtr) {
    // Leggi l'ASID 
    unsigned int current_asid = sPtr->sup_asid; 

    // Marca i frame occupati da tale Uproc come liberi 
    releaseFrames(current_asid); 

    // Se il processo era la shell fa V su masterSemaphore
    if (current_asid == SHELL_ASID) {
        SYSCALL(VERHOGEN, (unsigned int)&masterSemaphore, 0, 0);
    } 
    // Altrimenti fa V su shellSemaphore
    else {
        SYSCALL(VERHOGEN, (unsigned int)&shellSemaphore, 0, 0);
    }

    // Rilascia la Support Structure usata inserendola nella free list
    deallocateSupportStructure(sPtr);

    // Invoca la chiamata di sistema per terminare il processo
    SYSCALL(TERMPROCESS, 0, 0, 0); 
}

/**
 * @brief Verifica se l'indirizzo e' valido (e' all'interno del'area kuseg usato)
 * 
 * @param addr Indirizzo da verificare
 * @return int 1 se e' valido, 0 altrimenti
 */
static inline int isValidAddress(unsigned int addr) {
    // Area Text & Data: da 0x80000000 a 0x8001E000 (escluso)
    // Area Stack: una singola pagina da 0xBFFFF000 a 0xC0000000 (escluso)
    // Controlla se l'indirizzo è nell'area Text & Data o se l'indirizzo è nell'area Stack
    return ((addr >= 0x80000000 && addr < 0x8001E000) || (addr >= 0xBFFFF000 && addr < 0xC0000000)) ? 1 : 0;
}

/**
 * @brief Controlla se l'intervallo di memoria (buffer) si trova interamente 
 * all'interno di una singola area logica valida dell'Uproc.
 * 
 * @param startAddr Indirizzo logico di inizio
 * @param endAddr Indirizzo logico di fine (incluso)
 * @return int 1 se e' valido, 0 altrimenti
 */
static int isValidArea(unsigned int startAddr, unsigned int endAddr) {
    return (startAddr >= 0x80000000 && endAddr < 0x8001E000) || 
            (startAddr >= 0xBFFFF000 && endAddr < 0xC0000000) ? 1 : 0;
}

/**
 * @brief SYS4: WriteTerminal
 * Trasmette la stringa di caratteri sul terminale
 * 
 * @note Lunghezza massima di 128 caratteri
 * @note In caso di operazione andata a buon fine restituisce al chiamante numero di caratteri trasmessi,
 *       altrimenti mette un valore negativo (opposto dello status di errore)
 * @param sPtr Puntatore Support Structure del processo corrente che ha generato l'eccezione
 */
static void writeTerminal(support_t* sPtr) {
    char* virtAddr = (char *)sPtr->sup_exceptState[GENERALEXCEPT].reg_a1;
    int len = sPtr->sup_exceptState[GENERALEXCEPT].reg_a2;

    // Controllo sul parametro lunghezza passato
    if (len < 0 || len > 128) {
        terminate(sPtr);
        return;
    }

    // Se non c'e' nessun carattere da trasmettere
    if (len == 0) {
        sPtr->sup_exceptState[GENERALEXCEPT].reg_a0 = 0;
        return;
    }

    /* Controllo di validità sull'intera stringa (Inizio e Fine) */ 
    unsigned int startAddr = (unsigned int)virtAddr;
    unsigned int endAddr = (unsigned int)(virtAddr + len - 1);

    // Controllo validita' degli indirizzi (se nel range giusto)
    if (!isValidAddress(startAddr) || !isValidAddress(endAddr)) {
        terminate(sPtr);
        return;
    }

    // Controlla se la stringa inizi e finisca nella stessa area
    if (!isValidArea(startAddr, endAddr)) {
        terminate(sPtr);
        return;
    }
    
    // Ottieni semaforo per il terminal 0 in transmissione
    unsigned int semaphoreIndex = GET_IO_MUTEX_SEMAPHORE_INDEX(IL_TERMINAL, 0, 0);
    SYSCALL(PASSEREN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);

    termreg_t *term0reg = (termreg_t *)(DEV_REG_ADDR(IL_TERMINAL, 0));

    /* Trasmission della stringa */
    int transmitted;
    for (transmitted = 0; transmitted < len; transmitted++, virtAddr++) {
        unsigned int commandValue = (((unsigned int)*virtAddr) << 8) | PRINTCHR;
        int status = SYSCALL(DOIO, (int)&(term0reg->transm_command), (int)commandValue, 0);

        // Se c'e' stato errore nella trasmissione mette come valore di ritorno il valore negativo di status
        if ((status & TERMSTATMASK) != OKCHARTRANS) {
            sPtr->sup_exceptState[GENERALEXCEPT].reg_a0 = -status;
            SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);
            return;
        }
    }

    // Nel caso di trasmissione con successo mette come valore di ritorno il numero di caratteri trasmessi
    sPtr->sup_exceptState[GENERALEXCEPT].reg_a0 = transmitted;

    // Rilascio semaforo terminal 0 trasmissione
    SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);
}

/**
 * @brief SYS5: ReadTerminal
 * Legge dal terminale fino al carattere di fine riga '\n'
 * 
 * @note Viene salvato anche il carattere '\n'
 * @note In caso di operazione andata a buon fine restituisce al chiamante numero di caratteri ricevuti (compreso '\n'),
 *       altrimenti mette un valore negativo (opposto dello status di errore)
 * @param sPtr Puntatore Support Structure del processo corrente che ha generato l'eccezione
 */
static void readTerminal(support_t* sPtr) {
    // Leggiamo indirizzo inizio del buffer in cui memorizzare la stringa
    char* strAddr = (char *)sPtr->sup_exceptState[GENERALEXCEPT].reg_a1;

    // Controlla se indirizzo di inizio della stringa e' valida
    if (!isValidAddress((unsigned int)strAddr)) {
        terminate(sPtr);
        return;
    }

    // Acquisizione del semaforo del terminale 0 per trasmissione
    unsigned int semaphoreIndex = GET_IO_MUTEX_SEMAPHORE_INDEX(IL_TERMINAL, 0, 1);
    SYSCALL(PASSEREN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);

    termreg_t *term0reg = (termreg_t *)(DEV_REG_ADDR(IL_TERMINAL, 0));

    int received = 0;
    unsigned int commandValue = RECEIVECHAR;
    int status;

    /* Legge finche non trova il carratere \n */
    do
    {
        // Verifica se non si sta scrivendo in un area non permessa o se sta andando fuori dall'area logica
        // di partenza della stringa
        if (!isValidAddress((unsigned int)&strAddr[received]) || 
        !isValidArea((unsigned int)strAddr, (unsigned int)&strAddr[received])) {

            // Termina tale Uproc in tale caso
            SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);
            terminate(sPtr);
            return;
        }

        status = SYSCALL(DOIO, (int)&(term0reg->recv_command), (int)commandValue, 0);
        
        // Se e' fallita la lettura restituisce il valore negativo dello status
        if ((status & TERMSTATMASK) != RECVD ) {
            sPtr->sup_exceptState[GENERALEXCEPT].reg_a0 = -status;
            SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);
            return;
        }

        // Estrazione del carattere dai bit superiori (spostamento di 8 bit a destra)
        strAddr[received++] = (status >> 8) & TERMSTATMASK;
    } while((strAddr[received-1]) != '\n');

    // In caso di successo nella ricezione dell'intera riga restituisce il numero di caratteri ricevuti
    sPtr->sup_exceptState[GENERALEXCEPT].reg_a0 = received;

    // Rilascio semaforo terminal 0 ricezione
    SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);
}

/**
 * @brief SYS6: Execute
 * Comporta la creazione di un nuovo Uproc
 * 
 * @param sPtr Puntatore Support Structure del processo corrente che ha generato l'eccezione
 */
static void execute(support_t* sPtr) {
    // Leggiamo l'ASID del processo da spawnare dal registro a1
    int newAsid = (int)sPtr->sup_exceptState[GENERALEXCEPT].reg_a1;

    // Controllo di sicurezza sull'ASID 
    if (newAsid < 1 || newAsid > 8) {
        return;
    }

    /* Prepariamo lo state_t per il nuovo processo utente (U-proc) */  
    state_t newProcessState;
    
    // Configurazione dello stato iniziale dell'U-proc da spawnare
    // Stack Pointer all'interno del kuseg
    newProcessState.reg_sp = USERSTACKTOP;                    // Stack virtuale standard
    // PC punta all'inizio del segmento .text
    newProcessState.pc_epc = (memaddr)UPROCSTARTADDR;         // Entry point standard
    // Eseguito in user mode con tutti gli interrupt attivi
    newProcessState.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_U; // User Mode con interrupt vivi
    newProcessState.mie = MIE_ALL;

    // Inserimento ASID in EntryHI
    newProcessState.entry_hi = newAsid << ASIDSHIFT;          

    // Allocazione e inizializzazione della Support Structure 
    support_t *newSupport = allocateSupportStructure(newAsid);

    // Se allocazione di una Support Structure per il nuovo Uproc e' fallito
    if (!newSupport) {
        return;
    }

    // Lanciamo il nuovo processo tramite la CREATEPROCESS
    int pid = SYSCALL(CREATEPROCESS, (unsigned int)&newProcessState, PROCESS_PRIO_LOW, (unsigned int)newSupport);

    // Se la creazione del processo fallisce a livello di Nucleo
    if (pid < 0) {
        deallocateSupportStructure(newSupport);
        return;
    }

    // Blocca la Shell fino a quando il nuovo U-proc non farà una SYS2 (terminate).
    SYSCALL(PASSEREN, (unsigned int)&shellSemaphore, 0, 0);
}

/**
 * @brief Program Trap Handler
 * Effettua la stessa operazione di Terminate (SYS2)
 * 
 */
void programTrapHandler() {
    // Ottieni la struttura di supporto tramite la syscall negativa
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    // Invoca Terminate (SYS2)
    terminate(sPtr);
}