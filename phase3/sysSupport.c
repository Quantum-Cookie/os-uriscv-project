#include "sysSupport.h"
#include "initProc.h"

#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>

void generalSupportHandler() {}

#define SHELL_ASID 1

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