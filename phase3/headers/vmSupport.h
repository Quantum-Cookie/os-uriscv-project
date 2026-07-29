#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include "types.h"

/**
 * @brief Inizializza le strutture dati dello Swap Pool
 * Imposta il semaforo dello Swap Pool per garantire la mutua esclusione e configura
 * ogni elemento (frame fisico) della Swap Pool Table come libera, azzerando le associazioni
 * con le pagine logiche dei processi utente (U-proc)
 */
void initSwapStructs();

/**
 * @brief Pager: si occupa di caricare nella Swap Pool la pagina richiesta
 * 
 * La funzione viene invocata quando si verifica un'eccezione di tipo TLB-Invalid
 * (sia su operazione di lettura TLBL, sia di scrittura TLBS). Il suo compito è caricare
 * in RAM (nello Swap Pool) la pagina logica mancante recuperandola dal dispositivo Flash
 * associato all'U-proc corrente.
 */
void TLBPagerHandler();

/**
 * @brief Marca i frame del Uproc nella Swap Pool con ASID passato come liberi e ne pulisce i campi relativi
 * 
 * @param asid ASID del Uproc di cui si deve liberare i relativi frame nella Swap Pool
 */
void releaseFrames(int asid);

#endif