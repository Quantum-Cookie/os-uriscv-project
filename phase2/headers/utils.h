#ifndef UTILS_H
#define UTILS_H

#include <uriscv/cpu.h>
#include "types.h"

// Restituisce (evetualmente interrupt) exception code
#define GET_EXEC_CODE(cause) (((cause) & CAUSE_EXCCODE_MASK))

/**
 * @brief Funzione ausiliare per copiare struttura dati state_t
 * 
 * @param src Puntatore a state_t sorgente
 * @param dest Puntatore a state_t destinatario
 */
void copyState(state_t* src, state_t* dest);

/**
 * @brief Salva lo stato del processore viene salvato nell'apposito campo (p_s)
 *  e aggiona il tempo di utilizzo CPU accumulato del processo specificato
 * 
 * @param processorState Puntatore allo stato del processore prima dell'eccezione
 * @param process Puntatore al PCB del processo su cui effettuare aggiornamento dati
 */
void updateProcessState(state_t* processorState, pcb_t* process);

/**
 * @brief Funzione ausiliare per fare operazione di V su un semaforo
 * 
 * @param semAddr Puntatore al semaforo su cui effettuare l'operazione
 * @return pcb_t* Puntatore all'eventuale semaforo sbloccato dall'operazione
 */
pcb_t* vOnSem(int* semAddr);

#endif