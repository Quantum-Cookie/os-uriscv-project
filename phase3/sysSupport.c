#include "sysSupport.h"
#include "initProc.h"
#include "types.h"
#include "const.h"

#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include <uriscv/arch.h>

#define SHELL_ASID 1

#define GET_EXEC_CODE(cause) (((cause) & CAUSE_EXCCODE_MASK))

static void syscallHandler();

static void terminate(support_t* sPtr);
static void writeTerminal(support_t* sPtr);

void generalSupportHandler() {
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    unsigned int cause = sPtr->sup_exceptState[GENERALEXCEPT].cause;
    unsigned int excCode = GET_EXEC_CODE(cause);
    
    if (excCode == 8 || excCode == 11) {
        // SYSCALL
        syscallHandler(sPtr);
    } else {
        // Program Trap
        programTrapHandler();
    }
}

void syscallHandler() {
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    switch (sPtr->sup_exceptState[GENERALEXCEPT].reg_a0)
    {
    case TERMINATE:
        terminate(sPtr);
        break;
    case WRITETERMINAL:
        writeTerminal(sPtr);
        break;
    case READTERMINAL:
        break;
    case EXECUTE:
        break;
    default:
        break;
    }
}

static void terminate(support_t* sPtr) {
    // Leggi l'ASID 
    unsigned int current_asid = sPtr->sup_asid; 

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

static inline unsigned int getIOMutexSemaphoreIndex(unsigned int intExcCode, unsigned int devNo, unsigned int rx) {
    return ((intExcCode - 17) * 8) + devNo + (rx * 8); 
}

// Maschera per ottenere status dei device terminal
#define TERMSTATMASK 0xFF

static void writeTerminal(support_t* sPtr) {
    char* virtAddr = (char *)sPtr->sup_exceptState[GENERALEXCEPT].reg_a1;
    int len = sPtr->sup_exceptState[GENERALEXCEPT].reg_a2;

    if (len < 0 || len > 128) {
        terminate(sPtr);
        return;
    }

    unsigned int semaphoreIndex = getIOMutexSemaphoreIndex(IL_TERMINAL, 0, 0);
    SYSCALL(PASSEREN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);

    termreg_t *term0reg = (termreg_t *)(DEV_REG_ADDR(IL_TERMINAL, 0));

    int transmitted;
    for (transmitted = 0; transmitted < len; transmitted++, virtAddr++) {
        unsigned int commandValue = PRINTCHR | (((unsigned int)*virtAddr) << 8);
        int status = SYSCALL(DOIO, (int)&(term0reg->transm_command), (int)commandValue, 0);
        if ((status & TERMSTATMASK) != RECVD) {
            sPtr->sup_exceptState[GENERALEXCEPT].reg_a0 = -status;
            SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);
            return;
        }
    }
    sPtr->sup_exceptState[GENERALEXCEPT].reg_a0 = transmitted;

    SYSCALL(VERHOGEN, (int)&(suppIOMutexSemaphores[semaphoreIndex]), 0, 0);
}

void programTrapHandler() {
    // 1. Ottieni la struttura di supporto tramite la syscall negativa
    support_t *sPtr = (support_t *) SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    // 2. Leggi l'ASID 
    unsigned int current_asid = sPtr->sup_asid; 

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