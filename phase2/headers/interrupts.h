#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "types.h"

/**
 * @brief Funzione handler generale delle eccezioni
 * 
 * @param cause Valore del cause register
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 */
void deviceInterruptHandler(unsigned int cause, state_t* processorState);
void nonTimerInterrupts(unsigned int excCode, state_t* processorState);
void processorLocalTimerInt(state_t* processorState);
void intervalTimer(state_t* processorState);

#endif
