#ifndef INITAL_H
#define INITAL_H

#include "../../headers/listx.h"
#include "../../headers/types.h"
#include "../../headers/const.h"
#include "../../phase1/headers/pcb.h"
#include "../../phase1/headers/asl.h"
#include "./exceptions.h"
#include "./scheduler.h"

extern int processCount;
extern int softBlockCount;
extern struct list_head readyQueue;
extern pcb_t* currentProcess;
extern int deviceSemaphore[NRSEMAPHORES];


void initPassupvector();
void initNucleusData();
void initFirstProcess();

#endif // !INITAL_H
