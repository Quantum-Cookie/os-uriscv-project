#include "./headers/exceptions.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include "./headers/scheduler.h"

#include "../headers/listx.h"
#include "../headers/types.h"
#include "../headers/const.h"

#include "../phase1/headers/pcb.h"
#include "../phase1/headers/asl.h"
#include "./headers/initial.h"

// Restituisce l'exception code
#define GET_EXEC_CODE(cause) (((cause) & CAUSE_EXCCODE_MASK))

#define PROCESSOR_ID 0

static void deviceInterruptHandler();
static void tlbExceptionHandler();
static void syscallExceptionHandler(state_t* processorState);
static void programTrapExceptionHandler();

static void createProcess(state_t* processorState);
static void terminateProcess(state_t* processorState);
static void passren(state_t* processorState);
static void verhogen(state_t* processorState);


void exceptionHandler() {
    unsigned int cause = getCAUSE();
    
    // Verifica se l'eccezione sia un interrupt
    if (CAUSE_IS_INT(cause)) {
        deviceInterruptHandler();
    }
    else {
        switch (GET_EXEC_CODE(cause)) {
            case 24 ... 28:
                tlbExceptionHandler();
                break;

            case 8:
            case 11:
                state_t* processorState = GET_EXCEPTION_STATE_PTR(PROCESSOR_ID);
                syscallExceptionHandler(processorState);
                break;

            case 0 ... 7:
            case 9 ... 10:
            case 12 ... 23:
                programTrapExceptionHandler();
                break;

            default:
                break;
        } 
    }
}

void syscallExceptionHandler(state_t* processorState) {
    switch (processorState->reg_a0) {
        case CREATEPROCESS:
            createProcess(processorState);
            break;
        case TERMPROCESS:
            terminateProcess(processorState);
            break;
        case PASSEREN:
            passren(processorState);
            break;
        case VERHOGEN:
            verhogen(processorState);
            break;
    }
}

static void copyState(state_t* src, state_t* dest) {
    dest->entry_hi = src->entry_hi;
    dest->cause = src->cause;
    dest->status = src->status;
    dest->pc_epc = src->pc_epc;
    dest->mie = src->mie;

    // Copia i registri generici (GPR)
    for (int i = 0; i < STATE_GPR_LEN; i++) {
        dest->gpr[i] = src->gpr[i];
    }
}

static void createProcess(state_t* processorState) {
    pcb_t* newPcb = allocPcb();

    // Controllo se ci sono ancora PCB liberi
    if (!newPcb) {
        processorState->reg_a0 = -1;

        processorState->pc_epc += 4;
        LDST(processorState);
        return;
    }

    // p_time, p_semAdd, p_pid gia' inizializzati con allocPcb 
    //newPcb->p_s = *((state_t*)(processorState->reg_a1));
    copyState((state_t*)(processorState->reg_a1), &newPcb->p_s);

    if (processorState->reg_a3)
        newPcb->p_supportStruct = (support_t *)processorState->reg_a3;
    else
        newPcb->p_supportStruct = NULL;

    insertProcQ(&readyQueue, newPcb);
    insertChild(currentProcess, newPcb);
    processCount++;

    // Restituzione pid al chiamante
    processorState->reg_a0 = newPcb->p_pid;

    // Aggiorna PC e restituisce il controllo al chiamante
    processorState->pc_epc += 4;
    LDST(processorState);
}

/**
 * @brief Salva lo stato del processo attuale nell'apposito campo e aggiona il tempo di utilizzo CPU accumulato
 * 
 * @param processorState Puntatore allo stato del processore prima dell'eccezione
 */
static void updateProcessState(state_t* processorState, pcb_t* process) {
    cpu_t actTime;
    STCK(actTime);

    copyState(processorState, &process->p_s);
    process->p_time += actTime - startRunningTime;
}

static pcb_t* searchByPid(int pid, pcb_t* root) {
    if (root->p_pid == pid) 
        return root;

    pcb_t* child;
    list_for_each_entry(child, &root->p_child, p_sib) {
        pcb_t* res = searchByPid(pid, child);
        if (res)
            return res;
    }
    
    return NULL;
}

static void recursiveTermination(pcb_t* toTerminate) {
    while (!emptyChild(toTerminate))
    {
        // Itera per terminare tutti i processi figli
        pcb_t* childToTerminate = removeChild(toTerminate);
        recursiveTermination(childToTerminate);
    }
    if (!outProcQ(&readyQueue, toTerminate))
            outBlocked(toTerminate);
        freePcb(toTerminate);
}

static void terminateProcess(state_t* processorState) {
    int pid = processorState->reg_a1;

    if (pid == 0) {
        recursiveTermination(currentProcess);
    } 
    else {
        pcb_t* toTerminate = searchByPid(pid, rootProcess);
        if (toTerminate)
            recursiveTermination(toTerminate);

        processorState->pc_epc += 4;
        
        updateProcessState(processorState, currentProcess);

        insertProcQ(&readyQueue, currentProcess);
    }

    scheduler();
}

static void passren(state_t* processorState) {
    int* semadrr = (int*)processorState->reg_a1;
    (*semadrr)--;

    processorState->pc_epc += 4;

    if (*semadrr < 0) {
        updateProcessState(processorState, currentProcess);
        insertBlocked(semadrr, currentProcess);
        scheduler();
    }
    else {
        LDST(processorState);
    }
}

static void verhogen(state_t* processorState) {
    int* semadrr = (int*)processorState->reg_a1;
    (*semadrr)++;

    if (*semadrr <= 0) {
        pcb_t* readyProc = removeBlocked(semadrr);
        if (readyProc)
            insertProcQ(&readyQueue, readyProc);
    }

    processorState->pc_epc += 4;
    LDST(processorState);
}

void processorLocalTimerInt(state_t* processorState) {
    // Acknoledge interrupt e carica il nuovo valore
    setTIMER(TIMESLICE);

    // Salva lo stato attuale di esecuzione del processo
    updateProcessState(processorState, currentProcess);

    // Inserisce il processo nella Ready Queue
    insertProcQ(&readyQueue, currentProcess);

    scheduler();
}

static void deviceInterruptHandler() {
    unsigned int excCode = GET_EXEC_CODE(getCAUSE());

    state_t* processorState = GET_EXCEPTION_STATE_PTR(PROCESSOR_ID);
    if (excCode == IL_CPUTIMER)
        processorLocalTimerInt(processorState);
};

static void tlbExceptionHandler() {return;};
static void programTrapExceptionHandler() {return;};