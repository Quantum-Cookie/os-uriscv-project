#include "./headers/exceptions.h"
#include <uriscv/liburiscv.h>

void exceptionHandler() {
    unsigned int cause = getCAUSE();
    
    // Verifica se l'eccezione sia un interrupt
    if (CAUSE_IS_INT(cause)) {
        deviceInterruptHandler();
    }
    else {
        switch (cause) {
            case 24 ... 28:
                tlbExceptionHandler();
                break;

            case 8:
            case 11:
                state_t* processorState = GET_EXCEPTION_STATE_PTR(PROCESSOR_ID);
                syscallExceptionHandler(processorState);
                break;

            case 0 ... 7:
            case 9 ... 10:
            case 12 ... 23:
                programTrapExceptionHandler();
                break;

            default:
                break;
        } 
    }
}


void syscallExceptionHandler(state_t* processorState) {
    switch (processorState->reg_a0) {
        case -1:
            createProcess(processorState);
            break;
    }
}

void createProcess(state_t* processorState) {
    return;
}