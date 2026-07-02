#include "types.h"

#define GET_IO_MUTEX_SEMAPHORE_INDEX(intExcCode, devNo, rx) (((intExcCode - 17) * 8) + devNo + (rx * 8))

/* Variabili Globali */
extern int suppIOMutexSemaphores[NSUPPSEM];        // Array unico per tutti i semafori di mutua esclusione dei dispositivi
extern int masterSemaphore;                 // Semaforo per evitare che InstantiatorProcess termini prima che termini la shell, garantendo che il sistema rimanga attivo
extern int shellSemaphore;                  // Semaforo per fare si che la shell attenda la terminazione dei processi figli
extern unsigned int SWAP_POOL_START_ADDR;


support_t* allocateSupportStructure(int asid);

void deallocateSupportStructure(support_t* s);
void InstantiatorProcess();

void test();