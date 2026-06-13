#ifndef SCHEDULER_H
#define SCHEDULER_H

/**
 * @brief Scheduler preemptive round-robin basato sulla priorita' dei processi.
 * Estrae il prossimo processo dalla Ready Queue e lo carica. Gestisce
 * i casi di readyQueue vuota
 * 
 */
void scheduler();

#endif