#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

/**
 * @brief Funzione handler generale delle eccezioni
 * 
 */
void exceptionHandler();

/**
 * @brief Funzione che si occupa dell'evento TLB-Refill
 * Viene invocato quando una pagina non e' stata trovata dentro TLB (TLB-miss)
 * 
 */
void uTLB_RefillHandler();

#endif

