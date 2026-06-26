#include "types.h"

/* Variabili Globali */
extern int suppSemaphores[NSUPPSEM];        // Array unico per tutti i semafori di mutua esclusione dei dispositivi
extern int masterSemaphore;                 // Semaforo per evitare che InstantiatorProcess termini prima che termini la shell, garantendo che il sistema rimanga attivo
extern int shellSemaphore;                  // Semaforo per fare si che la shell attenda la terminazione dei processi figli

void test();