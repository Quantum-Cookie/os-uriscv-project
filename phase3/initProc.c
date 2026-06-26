#include "initProc.h"
#include "vmSupport.h"


/* Variabili globali */
// Array unico per tutti i semafori di mutua esclusione dei dispositivi
int suppSemaphores[NSUPPSEM];

// Semaforo per evitare che InstantiatorProcess termini prima che termini la shell, garantendo che il sistema rimanga attivo
int masterSemaphore = 0;

// Semaforo per fare si che la shell attenda la terminazione dei processi figli
int shellSemaphore = 0;


/* Variabili locali */ 


/**
 * @brief Funzione per inizializzare i semafori mutua esclusione dei dispositivi
 */
static void initSuppSemaphores() {
    for (int i = 0; i < NSUPPSEM; i++) suppSemaphores[i] = 1;
}

void test() {
    initSwapStructs();
    initSuppSemaphores();
}

