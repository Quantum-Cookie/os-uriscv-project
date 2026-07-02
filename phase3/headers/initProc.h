#ifndef INITPROC_H
#define INITPROC_H

#include "types.h"

/**
 * @brief Macro per ottenere indice del rispettivo semaforo del dispositivo
 * I/O per garantire la mutua esclusione.
 * 
 * @param intExcCode Interrupt Exception Code per tale device`
 * @param devNo Numero dispositvo 
 * @param rx Usato solo per terminal, 1 se e' receiver, 0 se e' transmitter. Tutti altri dispositivo 0
 * 
 */
#define GET_IO_MUTEX_SEMAPHORE_INDEX(intExcCode, devNo, rx) (((intExcCode - 17) * 8) + devNo + (rx * 8))

/* Variabili Globali */
// Array unico per tutti i semafori di mutua esclusione dei dispositivi I/O
extern int suppIOMutexSemaphores[NSUPPSEM];        

// Semaforo per evitare che InstantiatorProcess termini prima che termini la shell, garantendo che il sistema rimanga attivo
extern int masterSemaphore;                  

// Semaforo per bloccare la shell in attesa del comando di terminazione del figlio
extern int shellSemaphore;                  

// Indirizzo di inizio della Swap Pool (subito dopo area del sistema operativo)
extern unsigned int SWAP_POOL_START_ADDR;


/**
 * @brief Allocazione del Support Structure per un certo Uproc
 * 
 * @param asid ASID del Uproc di cui si vuole instanziare Support Structure
 * @return support_t* puntatore al Support Structure, NULL se non ce ne sono liberi
 */
support_t* allocateSupportStructure(int asid);

/**
 * @brief Reinserisce la Support Structure alla free list
 * 
 * @note Reinserisce senza pulire i campi
 */
void deallocateSupportStructure(support_t* s);

/**
 * @brief Processo iniziatore del Support Level.
 * Configura l'ambiente per l'esecuzione dei processi utente (U-proc):
 * determina i limiti della RAM, calcola la posizione dello Swap Pool, 
 * inizializza i semafori di mutua esclusione per i dispositivi di I/O 
 * e prepara la lista delle Support Structure libere. Infine, fa partire
 * shell si blocca in attesa della sua terminazione 
 */
void InstantiatorProcess();

#endif