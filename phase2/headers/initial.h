#ifndef INITAL_H
#define INITAL_H

#include "listx.h"
#include "types.h"
#include "const.h"

extern int processCount;
extern int softBlockCount;
extern struct list_head readyQueue;
extern pcb_t* currentProcess;
extern int deviceSemaphore[NRSEMAPHORES];

extern pcb_t* rootProcess;
extern cpu_t startRunningTime;

#endif // !INITAL_H
