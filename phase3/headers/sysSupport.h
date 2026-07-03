#ifndef SYSSUPPORT_H
#define SYSSUPPORT_H

/**
 * @brief Handler che gestisce tutte le eccezioni passati al Support Level non TLB
 * - Tutte le SYSCALL exceptions numberati >= 1
 * - Tutti i Program Trap exceptions
 */
void generalSupportHandler();

/**
 * @brief Program Trap Handler
 * Effettua la stessa operazione di Terminate (SYS2)
 * 
 */
void programTrapHandler();

#endif