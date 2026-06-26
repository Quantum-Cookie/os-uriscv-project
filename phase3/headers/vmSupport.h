#include "types.h"

/**
 * @brief Inizializza le strutture dati dello Swap Pool
 * Imposta il semaforo dello Swap Pool per garantire la mutua esclusione e configura
 * ogni elemento (frame fisico) della Swap Pool Table come libera, azzerando le associazioni
 * con le pagine logiche dei processi utente (U-proc)
 */
void initSwapStructs();

void TLBPagerHandler();