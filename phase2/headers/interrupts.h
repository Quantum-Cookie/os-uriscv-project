#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "../../headers/types.h"

void deviceInterruptHandler();
void nonTimerInterrupts(unsigned int excCode, state_t* processorState);
void processorLocalTimerInt(state_t* processorState);
void intervalTimer(state_t* processorState);

#endif
