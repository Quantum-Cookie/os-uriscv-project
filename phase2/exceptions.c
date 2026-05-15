#include "exceptions.h"

#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include <uriscv/types.h>

#include "scheduler.h"
#include "interrupts.h"
#include "listx.h"
#include "types.h"
#include "const.h"
#include "pcb.h"
#include "asl.h"
#include "initial.h"
#include "utils.h"

static void syscallExceptionHandler(state_t* processorState);

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

static void passUpOrDie(state_t* processorState, unsigned int index);

/*
void uTLB_RefillHandler() {
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t*) BIOSDATAPAGE);
}
*/

// Funzione handler generale delle eccezioni
void exceptionHandler() {
    // Legge il valore dell'exception salvato nel cause register
    unsigned int cause = getCAUSE();
    
    // BIOS Data Page per il processore 0
    state_t* processorState = GET_EXCEPTION_STATE_PTR(PROCESSOR_ID_0);

    // Verifica se l'eccezione sia un interrupt
    if (CAUSE_IS_INT(cause)) {
        deviceInterruptHandler(cause, processorState);
    }
    else {
        switch (GET_EXEC_CODE(cause)) {
            // TLB exceptions
            case 24 ... 28:
                passUpOrDie(processorState, PGFAULTEXCEPT);
                break;

            // SYSCALL
            case 8:
            case 11:
                syscallExceptionHandler(processorState);
                break;

            // Program Trap
            case 0 ... 7:
            case 9 ... 10:
            case 12 ... 23:
                passUpOrDie(processorState, GENERALEXCEPT);
                break;

            // Non ci dovrebbe mai arrivare
            default:
                break;
        } 
    }
}

/**
 * @brief Handler delle SYSCALL
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 * 
 */
void syscallExceptionHandler(state_t* processorState) {
    // Incremento del PC per evitare loop infinito di SYSCALL
    processorState->pc_epc += 4;

    // Nel registro a0 si trova il valore riferito alla SYSCALL richiesta
    int a0 = processorState->reg_a0;
    
    // Controlla se e' stata richiesta una SYSCALL, controllo preliminare in quanto le attuali SYSCALL sono solo per processi kernel
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
            }
        }
        // Altrimenti il processo corrente non ha i permessi e viene generato un Program Trap
        else {
            currentProcess->p_supportStruct->sup_exceptState[GENERALEXCEPT].cause = PRIVINSTR;
            passUpOrDie(processorState, GENERALEXCEPT);
        }
    }
    // Se il valore della SYSCALL e' >= 1 allora si cerca di passare il controllo al livello supporto specificato (se presente)
    else if (a0 >= 1) {
        passUpOrDie(processorState, GENERALEXCEPT);
    }
}


/**
 * @brief NSYS1: CreateProcess
 * Alloca un nuovo PCB e lo inizializza come figlio del processo corrente
 * Restituisce in a0 PID del nuovo processo in caso di successo, -1 altrimenti. 
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 * 
 */
static void createProcess(state_t* processorState) {
    /*
    - a1: Stato del processore del nuovo processo (state_t *)
    - a2: Priorita' del nuovo processo (int)
    - a3: Puntatore alla Support Structure (support_t *)
    */
    
    // p_time, p_semAdd, p_pid gia' inizializzati con allocPcb 
    pcb_t* newPcb = allocPcb();

    // Controllo se e' stato allocato un nuovo PCB, in caso negativo restituisco -1 e ritorno il controllo al chiamante
    if (!newPcb) {
        processorState->reg_a0 = -1;
        LDST(processorState);
        return;
    }

    // Imposto i parametri del nuovo processo con i dati passati
    copyState((state_t*)(processorState->reg_a1), &newPcb->p_s);
    newPcb->p_prio = processorState->reg_a2;
    newPcb->p_supportStruct = (support_t *)processorState->reg_a3;

    // Aggiunge il nuovo processo nella readyQueue per permettere la sua esecuzione
    insertProcQ(&readyQueue, newPcb);

    // Aggiunge il nuovo processo creato come figlio del chiamante della SYSCALL
    insertChild(currentProcess, newPcb);
    processCount++;

    // Restituzione PId del processo creato al chiamante
    processorState->reg_a0 = newPcb->p_pid;

    // Restituisce il controllo al chiamante
    LDST(processorState);
}

/**
 * @brief Funzione ausiliaria utilizzata per la ricerca di un PCB con pid in un albero/sottoalbero
 * 
 * @param pid Valore pid da ricercare
 * @param root Puntatore al punto di partenza della ricerca
 * @return pcb_t* Restituisce il puntatore al PCB ricercato, in caso di fallimento restituisce NULL
 * 
 */
static pcb_t* searchByPid(int pid, pcb_t* root) {
    // Se PCB ricercato e' root fa return immediato
    if (root->p_pid == pid) 
        return root;

    pcb_t* child;
    // Effettua una ricerca ricorsiva in profondita' sull'albero
    list_for_each_entry(child, &root->p_child, p_sib) {
        pcb_t* res = searchByPid(pid, child);
        if (res)
            return res;
    }
    
    // Caso fallimento ricerca
    return NULL;
}

/**
 * @brief Terminazione del processo passato e tutti i rispettivi figli
 * 
 * @param toTerminate Il processo padre di tutti i processi da terminare, anch'esso verra' terminato
 * @return int 1 se il processo corrente (currentProcess) è stato terminato, 0 altrimenti
 * 
 * @note Il processo `toTerminate` non viene rimosso dall'albero dei processi
 */
static int recursiveTermination(pcb_t* toTerminate) {
    // Serve per dire al chiamante se il processo corrente in esecuzione e' stato terminato o meno
    int terminatedCurrent = (toTerminate == currentProcess);

    // Itera per terminare tutti i processi figli
    while (!emptyChild(toTerminate))
    {
        pcb_t* childToTerminate = removeChild(toTerminate);
        terminatedCurrent += recursiveTermination(childToTerminate);
    }

    // Verifica se il processo terminato era bloccato su un semaforo
    if (toTerminate->p_semAdd) {
        int* savedSemAdd = toTerminate->p_semAdd; 
        outBlocked(toTerminate);

        int semIndex = savedSemAdd - &deviceSemaphore[0];
        // Se il processo era bloccato su un semaforo per dispositivi esterni
        if (semIndex >= 0 && semIndex < NRSEMAPHORES)
            // Decrementa il Soft-block Count
            softBlockCount--;

        // Se era bloccato per un semaforo utente
        else
            // Allora lo incrementa per evitare eventuali deadlock
           (*savedSemAdd)++;
    } else if (toTerminate != currentProcess) {
        outProcQ(&readyQueue, toTerminate);
    }

    // Decrementa Process Count e libera il PCB per poter essere utilizzato da un altro nuovo processo
    processCount--;
    freePcb(toTerminate);

    return (terminatedCurrent > 0);
}

/**
 * @brief NSYS2: TerminateProcess
 * Termina il processo in base al PID.
 * Se PID = 0 termina il processo chiamante, altrimenti il processo con PID passato
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 * 
 * @note Invoca sempre lo Scheduler al termine
 */
static void terminateProcess(state_t* processorState) {
    /*
    * a1: PID del processo da terminare (int)
    */

    int pid = processorState->reg_a1;
    int terminatedCurrent = 0;
    pcb_t* toTerminate =  NULL;

    // Se pid da terminare e' 0 allora deve terminare dal processo corrente, altrimenti lo ricerca nell'albero dei processi
    if (pid == 0) {
        toTerminate = currentProcess;
    }
    else {
        toTerminate = searchByPid(pid, rootProcess);
    }

    // Controlla se effettivamente esiste il processo da terminare
    if (toTerminate) {
        // lo rimuove dall'albero dei processi e lo termina insieme ai progeniti
        outChild(toTerminate);
        terminatedCurrent = recursiveTermination(toTerminate);
    }

    // Verifica se il processo corrente non sia stato terminato
    if (!terminatedCurrent) {
        // Aggiorna lo stato del processo in memoria con quello attuale e lo inserisce nella Ready Queue
        updateProcessState(processorState, currentProcess);
        insertProcQ(&readyQueue, currentProcess);
    }
    else {
        // Se e' stato terminato imposta NULL a currentProcess per mantenere coerenza
        currentProcess = NULL;
    }

    scheduler();
}

/**
 * @brief Funzione ausiliaria che effettua una P sul semaforo indicato
 * 
 * @param semAddr Puntatore al semaforo su cui effettuare l'operazione
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 * 
 * @note Per vedere se il processo dopo la P e' stata bloccata o meno bisogna verificare `currentProcess`
 */
static void pOnSem(int* semAddr, state_t* processorState) {
    (*semAddr)--;
    if (*semAddr < 0) {
        updateProcessState(processorState, currentProcess);
        insertBlocked(semAddr, currentProcess);

        currentProcess = NULL;
    }
}

/**
 * @brief NSYS3: Passeren (P)
 * Effettua una P sul semaforo passato in base al valore si potra' bloccare o meno
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 */
static void passeren(state_t* processorState) {
    /*
    * a1: Indirizzo del semaforo su cui effettuare operazione di P (int *)
    */

    int* semAdrr = (int*)processorState->reg_a1;
    
    pOnSem(semAdrr, processorState);

    // Se il processo si è bloccato, currentProcess è NULL e dobbiamo chiamare lo scheduler.
    if (currentProcess == NULL) {
        scheduler();
    // Se non si è bloccato, eseguiamo LDST per riprendere l'esecuzione del chiamante
    } else {
        LDST(processorState);
    }
}

/**
 * @brief NSYS4: Verhogen (V)
 * Effettua una V sul semaforo passato, se c'erano processi bloccati su tale semaforo verranno reinseriti in Ready Queue
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 */
static void verhogen(state_t* processorState) {
    /*
    * a1: Indirizzo del semaforo su cui effettuare operazione di V (int *)
    */
    
    int* semAddr = (int*)processorState->reg_a1;

    vOnSem(semAddr);

    LDST(processorState);
}

/**
 * @brief Funzione ausiliare che serve per mappare da indirizzi dei Device Registers Area agli indici dell'array dei semafori per dispositivi
 * 
 * @param addr Indirizzo appartenente ad Device Registers Area
 * @return unsigned int Indice del semaforo (compreso tra 1-48) del dispositivo che corrisponde all'indirizzo passato
 */
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
     * La mappatura rispetta ordine di priorita', piu' basso e' piu' e' alto la priorita'. Indice 0 apparte per Pesudo-clock per facilitare la gestione
     * e una maggiore chiarezza nella mappatura, altrimenti basterebbe fare shift tutto di 1 e mettere index 0 per Pesudo-clock.
     * 
     * Interval timer non ha un registro gestito con offset, quindi tale funzione non puo' fornire la mappatura per essa.
     */

    // devAddrBase = 0x10000054 + ((IntlineNo - 3) * 0x80) + (DevNo * 0x10)
    // offset rispetto all'inizio di Device Register Area
    unsigned int offset = addr - START_DEVREG;
    
    // Numero della linea di interrupt (3-7)    
    // I blocchi di linea hanno dimensione 0x80 byte uno
    unsigned int IntlineNo = (offset / 0x80) + 3;

    // Numero del dispositivo (0-7) all'interno della linea
    // Prima isoliamo l'offset relativo alla linea specifica, poi dividiamo per la dimensione del registro device (0x10)
    unsigned int DevNo = (offset % 0x80) / 0x10;

    unsigned int isRx = 0;
    if (IntlineNo == 7) {
        // Controlla offset per i terminali e verifica se sia una ricezione
        if (offset % 0x10 == 0x04)
            isRx = 1;
    }

    // (offset per classe di dispositivo) + (offset ulteriore per i sub devices di ricezione del terminale) + DevNo + 1
    return ((IntlineNo - 3) * 8) + (isRx * 8) + DevNo + 1;
}

/**
 * @brief NSYS 5: DoIO
 * Si supporta solo I/O sincroni quindi il processo passera' allo stato "blocked"
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 */
static void doIO(state_t* processorState) {
    /*
    * a1: Indirizzo del registro di comando del dispositivo (int *)
    * a2: Comando (int)
    */
    
    unsigned int commandAddr = processorState->reg_a1;
    unsigned int commandValue = processorState->reg_a2;

    // Scrivo il comando nel registro specificato
    *(unsigned int*)(commandAddr) = commandValue;

    // Identifico quale semaforo usare
    unsigned int semaphoreIndex = mapDeviceSemaphoreByAddr(commandAddr);

    // Faccio P sul semaforo trovato e incremento il Soft-block Count
    pOnSem(&deviceSemaphore[semaphoreIndex], processorState);
    softBlockCount++;

    scheduler();
}

/**
 * @brief NSYS6: GetCPUTime
 * Restituisce in a0 il tempo totalmente utilizzato dal processo chiamante in ms
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 */
static void GetCPUTime(state_t* processorState) {
    //Tempo di utilizzo CPU del processo corrente accumulato durante l'esecuzione attuale 
    cpu_t currentTime;
    STCK(currentTime);

    //Somma del tempo di esecuzione attuale con quello accumulato in precedenza
    cpu_t CPUTime = currentProcess -> p_time + (currentTime - startRunningTime); 

    //Memorizzazione del tempo di utilizzo CPU totale nel registro a0
    processorState->reg_a0 = CPUTime;

    //Ritorno all'esecuzione del chiamante
    LDST(processorState);
}

/**
 * @brief NSYS7: WaitForClock
 * Effettua una P sul semaforo per il Pesudo-clock. Passaggio allo stato "blocked" in attesa del prossimo Pesudo-clock tick
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 */
static void WaitForClock(state_t* processorState) {
    //Faccio P sul semaforo del Pesudo-clock
    pOnSem(&deviceSemaphore[PSEUDO_SEMAPHORE_INDEX], processorState);    

    softBlockCount++;
    scheduler();
}    

/**
 * @brief NSYS8: GetSupportData
 * Restituisce in a0 il valore di p_supportStruct se presente, altrimenti NULL
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 */
static void GetSupportData(state_t* processorState) {
    processorState->reg_a0 = (int)currentProcess->p_supportStruct;
    LDST(processorState);
}

/**
 * @brief NSYS9: GetProcessID
 * Restituisce in a0 PID del processo chiamante (0), altrimenti PID del processo padre (else)
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 */
static void GetProcessID(state_t* processorState) {
    /*
    * a1: 0 se si vuole sapere PID del processo chiamante, altrimenti PID del processo padre
    */

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
    
    // Ripristina lo stato del processore
    LDST(processorState);
}

/**
 * @brief NYSYS10: Yield
 * Rilascio volontario della CPU, non puo' essere scelto subito dallo Scheduler a meno che non ci siano altri processi in attesa nella Ready Queue
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 */
static void Yield(state_t* processorState) {
    //Aggiorno lo stato del processo corrente
    updateProcessState(processorState, currentProcess);

    // Se la Ready Queue non e' vuota
    if (!emptyProcQ(&readyQueue)) {
        // Estraiamo il processo in cima (quello che dovrebbe essere eseguito ora con la chiamata allo scheduler)
        pcb_t *nextToRun = removeProcQ(&readyQueue);
        
        // Inseriamo il processo corrente nella Ready Queue
        insertProcQ(&readyQueue, currentProcess);
        
        // Rimettiamo il processo 'nextToRun' esattamente in TESTA alla lista,
        // garantendo che sia scelto dallo scheduler alla chiamta
        list_add(&(nextToRun->p_list), &readyQueue);
    } else {
        // Altrimenti ci limitiamo ad inserirlo nella Ready Queue 
        insertProcQ(&readyQueue, currentProcess);
    }
    
    currentProcess = NULL;
    scheduler();
}

/**
 * @brief Pass Up or Die
 * Se p_supportStruct del processo corrente e' NULL viene terminato con i suoi progeniti, 
 * altrimenti l'eccezione viene passato alla routine specificata del Support Level
 * 
 * @param processorState Puntatore allo stato del processore nel momento dell'eccezione
 * @param index Indice per indicare se e' TLB exceptions (0) e non-TLB exceptions (1)
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
        // Copia exception state nel sup_exceptState indicato da index 
        copyState(processorState, &(currentProcess->p_supportStruct->sup_exceptState[index]));

        // Passa il controllo alla routine del Support Level specificato
        LDCXT(currentProcess->p_supportStruct->sup_exceptContext[index].stackPtr, 
            currentProcess->p_supportStruct->sup_exceptContext[index].status, 
            currentProcess->p_supportStruct->sup_exceptContext[index].pc);
    }
}
