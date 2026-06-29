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

#define GET_EXEC_CODE(cause) (((cause) & CAUSE_EXCCODE_MASK))

static void syscallHandler();

static void terminate(support_t* sPtr);
static void writeTerminal(support_t* sPtr);
static void readTerminal(support_t* sPtr);
static void execute(support_t* sPtr);

void generalSupportHandler() {
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    unsigned int cause = sPtr->sup_exceptState[GENERALEXCEPT].cause;
    unsigned int excCode = GET_EXEC_CODE(cause);
    
    if (excCode == 8 || excCode == 11) {
        // SYSCALL
        syscallHandler();
    } else {
        // Program Trap
        programTrapHandler();
    }
}

void syscallHandler() {
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
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

    LDST(&(sPtr->sup_exceptState[GENERALEXCEPT]));
}

static void terminate(support_t* sPtr) {
    // Leggi l'ASID 
    unsigned int current_asid = sPtr->sup_asid; 

    releaseFrames(current_asid); 

    // Ad esempio, esegui una V sul masterSemaphore o sul shellSemaphore a seconda di chi sta terminando
    // (come indicato nella sezione 7.1 per la terminazione ordinata)
    if (current_asid == SHELL_ASID) {
        SYSCALL(VERHOGEN, (unsigned int)&masterSemaphore, 0, 0);
    } else {
        SYSCALL(VERHOGEN, (unsigned int)&shellSemaphore, 0, 0);
    }

    // 4. Infine, invoca la chiamata di sistema NEGATIVA del Kernel per uccidere il processo
    SYSCALL(TERMPROCESS, 0, 0, 0); 
}

static inline int isValidAddress(unsigned int addr) {
    // Controlla se l'indirizzo è nell'area Text & Data o se l'indirizzo è nell'area Stack (1) altrimenti 0
    return ((addr >= 0x80000000 && addr < 0x8001E000) || (addr >= 0xBFFFF000 && addr < 0xC0000000)) ? 1 : 0;
}

static void writeTerminal(support_t* sPtr) {
    char* virtAddr = (char *)sPtr->sup_exceptState[GENERALEXCEPT].reg_a1;
    int len = sPtr->sup_exceptState[GENERALEXCEPT].reg_a2;

    if (len < 0 || len > 128) {
        terminate(sPtr);
        return;
    }

    // Controllo di validità sull'intera stringa (Inizio e Fine) 
    if (len > 0) {
        unsigned int startAddr = (unsigned int)virtAddr;
        unsigned int endAddr = (unsigned int)(virtAddr + len - 1);

        // NOTA: Aggiunto il '!' per intercettare gli indirizzi NON validi 
        if (!isValidAddress(startAddr) || !isValidAddress(endAddr)) {
            terminate(sPtr);
            return;
        }

        // Sicurezza extra: impedisce che una stringa inizi in Text&Data e finisca nello Stack 
        // (scavalcando l'area invalida in mezzo). Anche se con len max 128 è impossibile per via della distanza,
        // è una best practice verificare che entrambi gli indirizzi appartengano alla stessa macro-area.
        if ((startAddr < 0x8001E000 && endAddr >= 0x8001E000) ||
            (startAddr >= 0xBFFFF000 && endAddr < 0xBFFFF000)) {
            terminate(sPtr);
            return;
        }
    }

    unsigned int semaphoreIndex = GET_IO_MUTEX_SEMAPHORE_INDEX(IL_TERMINAL, 0, 0);
    SYSCALL(PASSEREN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);

    termreg_t *term0reg = (termreg_t *)(DEV_REG_ADDR(IL_TERMINAL, 0));

    int transmitted;
    for (transmitted = 0; transmitted < len; transmitted++, virtAddr++) {
        unsigned int commandValue = PRINTCHR | (((unsigned int)*virtAddr) << 8);
        int status = SYSCALL(DOIO, (int)&(term0reg->transm_command), (int)commandValue, 0);
        if ((status & TERMSTATMASK) != OKCHARTRANS) {
            sPtr->sup_exceptState[GENERALEXCEPT].reg_a0 = -status;
            SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);
            return;
        }
    }
    sPtr->sup_exceptState[GENERALEXCEPT].reg_a0 = transmitted;

    SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);
}

static int isValidArea(unsigned int startAddr, unsigned int endAddr) {
    return ((startAddr < 0x8001E000 && endAddr >= 0x8001E000) ||
            (startAddr >= 0xBFFFF000 && endAddr < 0xBFFFF000)) ? 0 : 1;
}

static void readTerminal(support_t* sPtr) {
    char* virtAddr = (char *)sPtr->sup_exceptState[GENERALEXCEPT].reg_a1;

    if (!isValidAddress((unsigned int)virtAddr)) {
        terminate(sPtr);
        return;
    }

    unsigned int semaphoreIndex = GET_IO_MUTEX_SEMAPHORE_INDEX(IL_TERMINAL, 0, 1);

    SYSCALL(PASSEREN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);

    termreg_t *term0reg = (termreg_t *)(DEV_REG_ADDR(IL_TERMINAL, 0));

    int received = 0;
    unsigned int commandValue = RECEIVECHAR;
    int status;
    do
    {
        if (!isValidAddress((unsigned int)&virtAddr[received]) || 
        !isValidArea((unsigned int)virtAddr, (unsigned int)&virtAddr[received])) {

            SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);
            terminate(sPtr);
            return;
        }

        status = SYSCALL(DOIO, (int)&(term0reg->recv_command), (int)commandValue, 0);
        
        if ((status & TERMSTATMASK) != RECVD ) {
            sPtr->sup_exceptState[GENERALEXCEPT].reg_a0 = -status;
            SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);
            return;
        }
        virtAddr[received++] = (status >> 8) & TERMSTATMASK;
    } while((virtAddr[received-1]) != '\n');

    sPtr->sup_exceptState[GENERALEXCEPT].reg_a0 = received;

    SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);
}

static void execute(support_t* sPtr) {
    // Leggiamo l'ASID del processo da spawnare dal registro a1
    int newAsid = (int)sPtr->sup_exceptState[GENERALEXCEPT].reg_a1;

    // Controllo di sicurezza sull'ASID 
    if (newAsid < 1 || newAsid > 8) {
        return;
    }

    // Prepariamo lo state_t per il nuovo processo utente (U-proc)
    state_t newProcessState;
    
    // Azzeriamo la struttura a blocchi di 32-bit per evitare dati spazzatura
    unsigned int *ptr = (unsigned int *)&newProcessState;
    for (int i = 0; i < (sizeof(state_t) / sizeof(unsigned int)); i++) {
        ptr[i] = 0;
    }

    // sConfigurazione dello stato iniziale dell'U-proc da spawnare
    newProcessState.reg_sp = USERSTACKTOP;                    // Stack virtuale standard
    newProcessState.pc_epc = (memaddr)UPROCSTARTADDR;         // Entry point standard
    newProcessState.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_U; // User Mode con interrupt vivi
    newProcessState.mie = MIE_ALL;
    newProcessState.entry_hi = newAsid << ASIDSHIFT;          // ASID nel registro MMU

    // Allocazione e inizializzazione della Support Structure tramite il tuo modulo
    support_t *newSupport = allocateSupportStructure(newAsid);

    // Lanciamo il nuovo processo tramite la CREATEPROCESS della Fase 2
    // Usiamo una priorità bassa (o quella standard prevista per i tuoi U-proc)
    int pid = SYSCALL(CREATEPROCESS, (unsigned int)&newProcessState, PROCESS_PRIO_LOW, (unsigned int)newSupport);

    // Se la creazione del processo fallisce a livello di Nucleo, ritorniamo l'errore alla Shell
    if (pid < 0) {
        return;
    }

    // BLOCCO DELLA SHELL (Come da specifica)
    // "During the handling of a SYS6 syscall, one can block the shell with a blocking P operation on the shellSemaphore"
    // Eseguiamo una PASSEREN sul semaforo globale della shell. 
    // Questo bloccherà la Shell qui dentro fino a quando il nuovo U-proc non farà una SYS2 (terminate).
    SYSCALL(PASSEREN, (unsigned int)&shellSemaphore, 0, 0);

    // Quando il processo figlio termina, qualcuno farà una V(shellSemaphore) sbloccando la Shell.
    // Una volta ripartiti, restituiamo 0 (o il pid del figlio terminato) in reg_a0 per indicare il successo.
    sPtr->sup_exceptState[GENERALEXCEPT].reg_a0 = 0;
}

void programTrapHandler() {
    // Ottieni la struttura di supporto tramite la syscall negativa
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    // Leggi l'ASID 
    unsigned int current_asid = sPtr->sup_asid; 

    releaseFrames(current_asid);

    // Ad esempio, esegui una V sul masterSemaphore o sul shellSemaphore a seconda di chi sta terminando
    // (come indicato nella sezione 7.1 per la terminazione ordinata)
    if (current_asid == SHELL_ASID) {
        SYSCALL(VERHOGEN, (unsigned int)&masterSemaphore, 0, 0);
    } else {
        SYSCALL(VERHOGEN, (unsigned int)&shellSemaphore, 0, 0);
    }

    // Infine, invoca la chiamata di sistema NEGATIVA del Kernel per uccidere il processo
    SYSCALL(TERMPROCESS, 0, 0, 0); 
}