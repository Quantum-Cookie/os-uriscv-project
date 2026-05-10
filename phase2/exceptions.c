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
static void syscallExceptionHandler(state_t* processorState);

static void passUpOrDie(state_t* processorState, unsigned int index);

static void createProcess(state_t* processorState);
static void terminateProcess(state_t* processorState);
static void passeren(state_t* processorState);
static void verhogen(state_t* processorState);
static void doIO(state_t* processorState);
static void GetCPUTime(state_t* processorState);
static void WaitForClock(state_t* processorState);
static void GetSupportData(state_t* processorState);
static void GetProcessID(state_t* processorState);
static void Yield (state_t* processorState);

void exceptionHandler() {
    unsigned int cause = getCAUSE();
    
    // Verifica se l'eccezione sia un interrupt
    if (CAUSE_IS_INT(cause)) {
        deviceInterruptHandler();
    }
    else {
        state_t* processorState = GET_EXCEPTION_STATE_PTR(PROCESSOR_ID);
        switch (GET_EXEC_CODE(cause)) {
            case 24 ... 28:
                passUpOrDie(processorState, PGFAULTEXCEPT);
                break;

            case 8:
            case 11:
                syscallExceptionHandler(processorState);
                break;

            case 0 ... 7:
            case 9 ... 10:
            case 12 ... 23:
                passUpOrDie(processorState, GENERALEXCEPT);
                break;

            default:
                break;
        } 
    }
}


void syscallExceptionHandler(state_t* processorState) {
    int a0 = processorState->reg_a0;
    if (-10 <= a0 && a0 <= -1) {
        // Controlla se il processo corrente e' in kernel mode
        if (((processorState->status & MSTATUS_MPP_MASK) == MSTATUS_MPP_M)) {
            switch (a0) {
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
                case GETTIME:
                    GetCPUTime(processorState);
                    break;
                case CLOCKWAIT:
                    WaitForClock(processorState);
                    break;
                case GETSUPPORTPTR:
                    GetSupportData(processorState);
                    break;
                case GETPROCESSID:
                    GetProcessID(processorState);
                    break;
                case YIELD:
                    Yield(processorState);
                    break;
                default:
                    /*if (a0 >= 1) {
                        processorState->pc_epc += 4;
                        passUpOrDie(processorState, GENERALEXCEPT);
                    }*/
                    break;
            }
        }
        else {
            processorState->pc_epc += 4;
            currentProcess->p_supportStruct->sup_exceptState[GENERALEXCEPT].cause = PRIVINSTR;
            passUpOrDie(processorState, GENERALEXCEPT);
        }
    }
    else if (a0 >= 1) {
        processorState->pc_epc += 4;
        passUpOrDie(processorState, GENERALEXCEPT);
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
        int* savedSemAdd = toTerminate->p_semAdd;  // salva PRIMA di outBlocked
        outBlocked(toTerminate);
        (*savedSemAdd)++;
        int semIndex = savedSemAdd - &deviceSemaphore[0];
        if (semIndex >= 0 && semIndex < NRSEMAPHORES) {
            softBlockCount--;
        }
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
    }
}

static void passeren(state_t* processorState) {
    int* semAdrr = (int*)processorState->reg_a1;
    processorState->pc_epc += 4;
    
    pOnSem(semAdrr, processorState);

    // Se il processo si è bloccato, currentProcess è NULL e dobbiamo chiamare lo scheduler.
    // Se non si è bloccato, eseguiamo LDST.
    if (currentProcess == NULL) {
        scheduler();
    } else {
        LDST(processorState);
    }
}

static pcb_t* vOnSem(int* semAddr) {
    (*semAddr)++;
    pcb_t* readyProc = NULL;
    if (*semAddr <= 0) {
        readyProc = removeBlocked(semAddr);
        if (readyProc)
            insertProcQ(&readyQueue, readyProc);

    }
    return readyProc;
}

static void verhogen(state_t* processorState) {
    int* semAddr = (int*)processorState->reg_a1;

    vOnSem(semAddr);

    processorState->pc_epc += 4;
    LDST(processorState);
}

static unsigned int mapDeviceSemaphoreByAddr(unsigned int addr) {
    /*** 
     * Index 0: intlineNo 2
     * Index 1-8: intlineNo 3
     * Index 9-16: intlineNo 4
     * Index 17-24: intlineNo 5
     * Index 25-32: intlineNo 6
     * Index 33-40: intlineNo 7 - tx
     * Index 41-48: intlineNo 7 - rx
     * 
     * La mappatura rispetta ordine di priorita', piu' basso e' piu' e' prioritario apparte per Interval timer per facilitare la gestione
     * e una maggiore chiarezza nella mappatura, altrimenti basterebbe fare shift tutto di 1 e mettere index 0 per Interval timer.
     * 
     * Interval timer non ha un registro gestito con offset, quindi tale funzione non puo' fornire la mappatura per essa.
     */

    // Ci sono calcoli ridondanti come il +3 messo per IntlineNo, lo si ha lasciato per maggiore chiarezza
    unsigned int offset = addr - START_DEVREG;
    unsigned int IntlineNo = (offset / 0x80) + 3;
    unsigned int DevNo = (offset % 0x80) / 0x10;

    unsigned int isRx = 0;
    if (IntlineNo == 7) {
        // Controlla offset per i terminali e verifica se sia una ricezione
        if (offset % 0x10 == 0x04)
            isRx = 1;
    }

    // (offset per classe di dispositivo) + (offset ulteriore per i sub devices di ricezione del terminale) + DevNo + 1
    // DevNo va da 0 a 7
    return ((IntlineNo - 3) * 8) + (isRx * 8) + DevNo + 1;
}

static void doIO(state_t* processorState) {
    // SYSCALL(DOIO, int *commandAddr, int commandValue, 0);
    
    // Scrivo il comando nel registro specificato
    *(unsigned int*)(processorState->reg_a1) = processorState->reg_a2;

    // Identifico quale semaforo usare
    unsigned int semaphoreIndex = mapDeviceSemaphoreByAddr(processorState->reg_a1);

    processorState->pc_epc += 4;

    // Faccio P sul semaforo trovato
    pOnSem(&deviceSemaphore[semaphoreIndex], processorState);
    if (currentProcess == NULL) softBlockCount++;
    scheduler();
}

//(ancora da testare)
static void GetCPUTime(state_t* processorState){
    //Tempo di utilizzo CPU del processo corrente accumulato durante l'esecuzione attuale 
    cpu_t currentTime;
    STCK(currentTime);
    //Somma del tempo di esecuzione attuale con quello accumulato in precedenza
    cpu_t CPUTime = currentProcess -> p_time + (currentTime - startRunningTime); 
    //Memorizzazione del tempo di utilizzo CPU totale nel registro a0
    processorState->reg_a0 = CPUTime;
    //Aggiornamento del PC e ritorno all'esecuzione del chiamante
    processorState->pc_epc += 4;
    LDST(processorState);
}
//(ancora da testare)
static void WaitForClock(state_t* processorState) {
    //Aggiornamento del PC
    processorState->pc_epc += 4;
    //Faccio P sul semaforo del pseudo clock
    pOnSem(&deviceSemaphore[PSEUDO_SEMAPHORE_INDEX], processorState);    
    if (currentProcess == NULL) softBlockCount++;
    scheduler();
}    
//(ancora da testare)
static void GetSupportData(state_t* processorState) {
    if (currentProcess->p_supportStruct) {
        processorState->reg_a0 = (int)currentProcess->p_supportStruct;
    } else {
        processorState->reg_a0 = (int)NULL;
    }
    processorState->pc_epc += 4;
    LDST(processorState);
}

//(ancora da testare)
static void GetProcessID(state_t* processorState) {
    // Controlla il parametro nel registro a1
    if (processorState->reg_a1 == 0) {
        // Restituisce il PID del processo chiamante
        processorState->reg_a0 = currentProcess->p_pid;
    } else {
        // Gestione per prevenire crash se il padre è NULL (es. root process)
        if (currentProcess->p_parent != NULL) {
            processorState->reg_a0 = currentProcess->p_parent->p_pid;
        } else {
            processorState->reg_a0 = 0; // Il genitore della radice è 0
        }
    }
    
    // Incremento del PC per evitare il loop della SYSCALL
    processorState->pc_epc += 4;
    
    // Ripristina lo stato del processore
    LDST(processorState);
}

//(ancora da testare)
static void Yield (state_t* processorState){
    //Aggiornamento del PC
    processorState->pc_epc += 4;
    //Salvo lo stato del processo corrente
    updateProcessState(processorState, currentProcess);

    if (!emptyProcQ(&readyQueue)) {
        // Estraiamo il processo in cima (quello che dovrebbe essere eseguito ora)
        pcb_t *nextToRun = removeProcQ(&readyQueue);
        
        // Inseriamo il processo corrente rispettando la sua priorità.
        // Se è a priorità altissima, tornerà in cima alla readyQueue (dietro ad altri di pari livello).
        insertProcQ(&readyQueue, currentProcess);
        
        // Rimettiamo il processo 'nextToRun' esattamente in TESTA alla lista,
        // garantendo che sia LUI ad essere scelto dallo scheduler tra un istante.
        list_add(&(nextToRun->p_list), &readyQueue);
    } else {
        // Se è l'unico processo nel sistema, riparte lui come da specifica
        insertProcQ(&readyQueue, currentProcess);
    }
    
    //Faccio partire lo scheduler
    currentProcess = NULL;
    scheduler();
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

    currentProcess = NULL;

    scheduler();
}

void nonTimerInterrupts(unsigned int excCode, state_t* processorState) {
    if (currentProcess) {
        cpu_t actTime;
        STCK(actTime);
        currentProcess->p_time += actTime - startRunningTime;
    }

    //STCK(startRunningTime);
    unsigned int IntlineNo = excCode - 14;
    
    // 1. Trova il bit del dispositivo (Priorità: bit più basso)
    unsigned int *bitmapAddr = (unsigned int *)(0x10000040 + (IntlineNo - 3) * 0x04);

    int devNo;
    for (devNo = 0; devNo < 8 && !(*bitmapAddr & (1 << devNo)); devNo++);
    
    if (devNo == 8) return;

    // 2. Calcola l'indirizzo base e prepara le variabili
    memaddr devAddrBase = START_DEVREG + ((IntlineNo - 3) * 0x80) + (devNo * 0x10);
    int semIndex = (IntlineNo - 3) * 8 + devNo + 1;
    unsigned int status;

    if (excCode == IL_TERMINAL) { 
        termreg_t *termReg = (termreg_t *)devAddrBase;
        
        if (termReg->transm_status != UNINSTALLED && termReg->transm_status != READY && termReg->transm_status != BUSY) {
            status = termReg->transm_status;
            termReg->transm_command = ACK; 
        } else {
            status = termReg->recv_status;
            termReg->recv_command = ACK;  
            semIndex += 8;              
        }
    } else { 
        dtpreg_t *devReg = (dtpreg_t *)devAddrBase;
        status = devReg->status;
        devReg->command = ACK;            
    }

    pcb_t* readyProc = vOnSem(&deviceSemaphore[semIndex]);
    if (readyProc) {
        readyProc->p_s.reg_a0 = status;
        softBlockCount--;  // era bloccato su device
    }
    
    if (currentProcess) {
        STCK(startRunningTime);
        LDST(processorState);
    }
    else {
        scheduler();
    }
}

void intervalTimer(state_t* processorState) {
    // Ricarica l'interval timer
    LDIT(PSECOND);

    if (currentProcess) {
        cpu_t actTime;
        STCK(actTime);
        currentProcess->p_time += actTime - startRunningTime;
    }

    // Sblocca tutti i processi in attesa del pseudo-clock (V fino a 0)
    while (deviceSemaphore[PSEUDO_SEMAPHORE_INDEX] < 0) {
        vOnSem(&deviceSemaphore[PSEUDO_SEMAPHORE_INDEX]);
        softBlockCount--;
    }
    if (currentProcess) {
        STCK(startRunningTime);
        LDST(processorState);
    }
    else
        scheduler();
}

static void deviceInterruptHandler() {
    unsigned int excCode = GET_EXEC_CODE(getCAUSE());

    state_t* processorState = GET_EXCEPTION_STATE_PTR(PROCESSOR_ID);
    switch (excCode)
    {
        case IL_CPUTIMER:
            processorLocalTimerInt(processorState);
            break;
        case IL_TIMER:
            intervalTimer(processorState);
            break;
        case IL_DISK:
        case IL_FLASH:
        case IL_ETHERNET:
        case IL_PRINTER:
        case IL_TERMINAL:
            nonTimerInterrupts(excCode, processorState);
            break;
        default:
            break;
    }        
};

/****
 * END
 */


static void passUpOrDie(state_t* processorState, unsigned int index) {
    // Se il puntatore p_supportStruct e' nullo termina il processo e tutti i suoi progeniti
    if (!currentProcess->p_supportStruct) {
        outChild(currentProcess);
        recursiveTermination(currentProcess);
        currentProcess = NULL;

        scheduler();
    }
    else {
        copyState(processorState, &(currentProcess->p_supportStruct->sup_exceptState[index]));
        LDCXT(currentProcess->p_supportStruct->sup_exceptContext[index].stackPtr, 
            currentProcess->p_supportStruct->sup_exceptContext[index].status, 
            currentProcess->p_supportStruct->sup_exceptContext[index].pc);
    }
}
