#ifndef UTILS_H
#define UTILS_H

#include <uriscv/cpu.h>
#include "types.h"


void copyState(state_t* src, state_t* dest);
void updateProcessState(state_t* processorState, pcb_t* process);
pcb_t* vOnSem(int* semAddr);

#endif