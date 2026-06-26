#include "vmSupport.h"

#define SWAP_POOL_SIZE (2 * UPROCMAX)

// Swap Pool Table: contiene informazioni riguardante la pagina logica che occupa un cella 
swap_t swapPoolTable[SWAP_POOL_SIZE];

// Swap Pool Semaphore: serve per garantire la mutua esclusione nell'accesso della Swap Pool Table
int swapPoolSemaphore;


// Funzione per inizializzare Inizializza le strutture dati dello Swap Pool
void initSwapStructs() {
    /* Inizializza il semaforo di mutua esclusione dello Swap Pool a 1 */
    swapPoolSemaphore = 1;
    
    for (int i = 0; i < SWAP_POOL_SIZE; i++) {
        /* Un ASID pari a NOPROC (-1) indica che il frame nello Swap Pool è libero */
        swapPoolTable[i].sw_asid = NOPROC;
        
        /* Inizializza il Virtual Page Number (VPN) a 0 in quanto il frame non ospita pagine */
        swapPoolTable[i].sw_pageNo = 0;
        
        /* Imposta a NULL il puntatore alla Page Table Entry (PTE) */
        swapPoolTable[i].sw_pte = NULL;
    }
}

void TLBPagerHandler(){}