#ifndef INITAL_H
#define INITAL_H

#include "listx.h"
#include "types.h"
#include "const.h"

/* Variabili Globali */
extern int processCount;                    // Numero dei processi attualmente attivi
extern int softBlockCount;                  // Numero dei processi in attesa di qualche evento (i.e. I/O, timer)
extern struct list_head readyQueue;         // Coda dei PCB ready, ovvero che possono essere scelti dallo scheduler per l'esecuzione
extern pcb_t* currentProcess;               // Puntatore al PCB del processo in attuale esecuzione
extern int deviceSemaphore[NRSEMAPHORES];   // Array di interi come semaforo per ogni dispositivo esterno

extern pcb_t* rootProcess;                  // Il primo processo che viene instanziato, il quale rappresenta la radice dell'intero albero dei processi
extern cpu_t startRunningTime;              //Memorizza il timestamp del momento in cui currentProcess inizia la sua esecuzione per calcolare tempo totale di CPU consumato

#endif
