#include "./headers/exceptions.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include <uriscv/types.h>

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
static void passeren(state_t* processorState);
static void verhogen(state_t* processorState);
static void doIO(state_t* processorState);

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
            passeren(processorState);
            break;
        case VERHOGEN:
            verhogen(processorState);
            break;
        case DOIO:
            doIO(processorState);
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

    newPcb->p_prio = processorState->reg_a2;

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

static int recursiveTermination(pcb_t* toTerminate) {
    int terminatedCurrent = (toTerminate == currentProcess);

    while (!emptyChild(toTerminate))
    {
        // Itera per terminare tutti i processi figli
        pcb_t* childToTerminate = removeChild(toTerminate);
        terminatedCurrent += recursiveTermination(childToTerminate);
    }

    if (toTerminate->p_semAdd) {
        outBlocked(toTerminate);
        softBlockCount--;
    } else if (toTerminate != currentProcess) {
        outProcQ(&readyQueue, toTerminate);
    }

    processCount--;
    freePcb(toTerminate);

    return (terminatedCurrent > 0);
}

static void terminateProcess(state_t* processorState) {
    int pid = processorState->reg_a1;
    int terminatedCurrent = 0;
    pcb_t* toTerminate =  NULL;

    if (pid == 0) {
        toTerminate = currentProcess;
    }
    else {
        toTerminate = searchByPid(pid, rootProcess);
    }

    if (toTerminate) {
        outChild(toTerminate);
        terminatedCurrent = recursiveTermination(toTerminate);
    }

    if (!terminatedCurrent) {
        processorState->pc_epc += 4;
        
        updateProcessState(processorState, currentProcess);
    
        insertProcQ(&readyQueue, currentProcess);
    }
    else {
        currentProcess = NULL;
    }

    scheduler();
}

static void pOnSem(int* semAddr, state_t* processorState) {
    (*semAddr)--;
    if (*semAddr < 0) {
        updateProcessState(processorState, currentProcess);
        insertBlocked(semAddr, currentProcess);

        currentProcess = NULL;
        scheduler();
    }
}

static void passeren(state_t* processorState) {
    int* semAdrr = (int*)processorState->reg_a1;
    processorState->pc_epc += 4;
    
    pOnSem(semAdrr, processorState);

    LDST(processorState);
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

static unsigned int mapDeviceSemaphore(unsigned int addr) {
    /*** 
     * Index 0-7: intlineNo 3
     * Index 8-15: intlineNo 4
     * Index 16-23: intlineNo 5
     * Index 24-31: intlineNo 6
     * Index 32-39: intlineNo 7 - tx
     * Index 40-47: intlineNo 7 - rx
     * Index 48: intlineNo 2
     * 
     * La mappatura rispetta ordine di priorita', piu' basso e' piu' e' prioritario apparte per Interval timer per facilitare la gestione
     * e una maggiore chiarezza nella mappatura, altrimenti basterebbe fare shift tutto di 1 e mettere index 0 per Interval timer.
     * 
     * Interval timer non ha un registro gestito con offset, quindi tale funzione non puo' fornire la mappatura per essa.
     */

    // Ci sono calcoli ridondanti come il +3 messo per IntlineNo, lo si ha lasciato per maggiore chiarezza
    unsigned int offset = addr - 0x10000054;
    unsigned int IntlineNo = (offset / 0x80) + 3;
    unsigned int DevNo = (offset % 0x80) / 0x10;

    unsigned int isRx = 0;
    if (IntlineNo == 7) {
        // Controlla offset per i terminali e verifica se sia una ricezione
        if (offset % 0x10 == 0x04)
            isRx = 1;
    }

    // (offset per classe di dispositivo) + (offset ulteriore per i sub devices di ricezione del terminale) + DevNo
    // DevNo va da 0 a 7
    return ((IntlineNo - 3) * 8) + (isRx * 8) + DevNo;
}

static void doIO(state_t* processorState) {
    // SYSCALL(DOIO, int *commandAddr, int commandValue, 0);
    
    // Scrivo il comando nel registro specificato
    *(unsigned int*)(processorState->reg_a1) = processorState->reg_a2;

    // Identifico quale semaforo usare
    unsigned int semaphoreIndex = mapDeviceSemaphore(processorState->reg_a1);

    processorState->pc_epc += 4;

    // Faccio P sul semaforo trovato
    pOnSem(&deviceSemaphore[semaphoreIndex], processorState);
}


/****
 * DA SPOSTARE IN INTERRUPT.C
 */
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

/****
 * END
 */

static void tlbExceptionHandler() {return;};
static void programTrapExceptionHandler() {return;};