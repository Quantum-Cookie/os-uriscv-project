#include "sysSupport.h"
#include "initProc.h"
#include "types.h"

#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>

#define SHELL_ASID 1

#define GET_EXEC_CODE(cause) (((cause) & CAUSE_EXCCODE_MASK))

static void syscallHandler();

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
        break;
    case WRITETERMINAL:
        break;
    case READTERMINAL:
        break;
    case EXECUTE:
        break;
    default:
        break;
    }
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