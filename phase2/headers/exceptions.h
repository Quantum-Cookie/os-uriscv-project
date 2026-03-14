#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include "../../headers/listx.h"
#include "../../headers/types.h"
#include "../../headers/const.h"

// Controlla se il bit piu' significativo della causa sia 1 o meno
#define CAUSE_IS_INT(cause) (((cause) & 0x80000000) != 0)

#define PROCESSOR_ID 0

void exceptionHandler();
void deviceInterruptHandler();
void tlbExceptionHandler();
void syscallExceptionHandler(state_t* processorState);
void programTrapExceptionHandler();

void createProcess(state_t* processorState);

#endif // !EXCEPTIONS_H

