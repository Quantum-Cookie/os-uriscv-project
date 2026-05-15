# PandOSsh - Fase 2


## Variabili Globali
Sono state introdotte una serie di variabili condivise tra i diversi moduli della fase 2:

`int processCount`: Rappresenta il numero dei processi attualmente attivi.

`int softBlockCount`: Rappresenta il numero dei processi in attesa di qualche evento (i.e. I/O, timer).

`struct list_head readyQueue`: Coda dei PCB ready, ovvero che possono essere scelti dallo scheduler per l'esecuzione.

`pcb_t* currentProcess`: Puntatore al PCB del processo in attuale esecuzione.

`int deviceSemaphore[NRSEMAPHORES]`: Array di interi come semaforo per ogni dispositivo esterno. Seguendo la seguente mappatura:

| Index       | Device Class               | IntlineNo |
| :---------- | :------------------------- | :-------- |
| **0**       | Interval Timer             | 2         |
| **1 – 8**   | Disk Devices               | 3         |
| **9 – 16**  | Flash Devices              | 4         |
| **17 – 24** | Network (Ethernet) Devices | 5         |
| **25 – 32** | Printer Devices            | 6         |
| **33 – 40** | Terminal Devices (TX)      | 7         |
| **41 – 48** | Terminal Devices (RX)      | 7         |

`pcb_t* rootProcess`: Il primo processo che viene instanziato, il quale rappresenta la radice dell'intero albero dei processi.

`cpu_t startRunningTime`: Memorizza il timestamp del momento in cui **currentProcess** inizia la sua esecuzione per calcolare tempo totale di CPU consumato.

## Costanti

Sono state aggiunte delle costanti nel file `headers/const.h`:
- `PROCESSOR_ID_0: 0`: Rappresenta id del processore 0. 
- `PSEUDO_SEMAPHORE_INDEX: 0`: Indice del semaforo utilizzato dal Pseudo-clock nell'array di semafori dedicato ai dispositivi esterni.

## Utilità variabile `rootProcess`

La variabile globale `rootProcess` è stata introdotta per facilitare la ricerca del PCB con un id specifico. Utilizzato nella SYSCALL **NSYS2** quando il pid del processo da terminare sia diverso da 0.

Tale approccio è coerente con le specifiche del progetto in quanto:
- Se un processo padre termina allora anche tutti i suoi progeniti devono terminare. Conseguentemente se il **rootProcess** fosse stato terminato per qualunque ragione allora tutti i processi istanziati saranno terminati;
- Il **main** per iniziallizzazione dopo che è stato chiamato lo scheduler non dovrebbe essere più eseguito, quindi il **rootProcess** dev'essere per forza uno solo;
- Tutti i processi attivi, in qualunque stato siano devono essere per progettazione nell'albero.

Un altra possibilità, sarebba quella di usare una lista doppiamente concatenata circolare di tutti i processi attivi in quel momento: tale lista dovrebbe contenere i processi nella coda readyReady, quelli bloccati, e quello in esecuzione. Tuttavia bisognerebbe ricordarsi di aggiornare la lista per mantenerlo coerente alla situazione reale e aggiungere un campo al PCB in modo da collegarlo a tale lista.

Utilizzando invece la variabile `rootProcess` è sufficiente memorizzarlo una volta e non serve più modificarlo. Entrambe le scelte hanno lo stesso costo per ricerca: `O(n)` con n il numero di processi attualmente attivi.


## Inizializzazione del nucleo

L'inizializzazione rappresenta il punto di ingresso del sistema e si occupa di preparare l'ambiente prima di cedere il controllo allo Scheduler.
Vengono inizializzate tutte le variabili globali e le strutture dati del Livello 2 (PCB, ASL) permettendone il loro utilizzo. 

Particolare rilievo spetta alla configurazione del Processor 0 Pass Up Vector (all'indirizzo `0x0FFFF900`), il quale serve al BIOS per chiamare il handler relativo a **TLB-Refill** ed **Eccezioni**. Per entrambi gli handler, lo stack viene impostato all'indirizzo `0x2000.1000`.

Infine viene istanziato il primo processo (`rootProcess`) con stack pointer a `RAMTOP`.


## Scheduler

Lo scheduler `preemptive` implementato è `round-robin` a priorità, con un time slice di **5ms**. 
La priorità è dovuta alla progettazione e all'implementazione in fase 1 dei PCB, quest'ultimi nella coda vengono ordinate in modo decrescente in base alla loro priorità. Lo scheduler semplicemente si limita a rimuovere il processo in testa da `readyQueue`, che sarebbe quello con priorità maggiore.

Tuttavia la priorità è statica, non ci sono meccanismi per modificare la priorità dei processi, ne SYSCALL ne sistemi come aging. Quindi risulta inevitabile che ci possano essere situazioni di starvation.

Lo scheduler si occupa anche di ricaricare il `PLT` di **5ms**, e di aggiornare `startRunningTime` quando inizia l'esecuzione del processo.

Quando la coda `readyQueue` risulta vuota, in base allo stato attuale lo scheduler può:
- Se `processCount == 0`: Viene considerato lavoro ben fatto, e invoca `HALT`.
- Se `processCount > 0 && softBlockCount > 0`: Significa che tutti i processi sono in attesa di qualche evento, e il sistema dopo aver disattivato l'interrupt del PLT (evitando di risvegliarsi per niente) entra in `Wait State`.
- Se `processCount > 0 && softBlockCount == 0`: Significa che c'è deadlock in quanto ci sono processi in attesa ma nessuno di questi sta attendendo eventi esterni, e invoca `PANIC`.


## Exceptions

La funzione che rappresenta il handler generale delle eccezioni si deve occupare di smistare i vari casi in base al **exception code** che si ottiene tramite la funzione `getCause()`:

- se la macro `CAUSE_IS_INT` restituisce true allora e' un **interrupt** - `deviceInterruptHandler`
- 24-28: TLB exceptions - `passUpOrDie`
- 8-11: SYSCALL - `syscallExceptionHandler`
- 0-7, 9, 10, 12-23: program trap `passUpOrDie`


## SYSCALL
La chiamata è fatta secondo la sintassi:

```c
SYSCALL(a0, a1, a2, a3);
```
In `a0` si trova il valore della SYSCALL chiamata, mentre in `a1, a2, a3` gli eventuali parametri richiesti per poter soddisfare la richiesta.

Tutte le SYSCALL implementate (-1 a -10) sono progettate per essere utilizzate solo dai processi privilegiati (**kernel-mode**):

| Valore (a0) | Nome Servizio      | Descrizione                                                                                                    |
| :---------- | :----------------- | :------------------------------------------------------------------------------------------------------------- |
| **-1**      | `CREATEPROCESS`    | Crea un processo figlio con i parametri forniti. Restituisce il PID o -1 se mancano PCB.                       |
| **-2**      | `TERMINATEPROCESS` | Termina il processo specificato in `a1` (0 per se stesso) e tutti i suoi figli.                                |
| **-3**      | `PASSEREN (P)`     | Esegue un'operazione P sul semaforo all'indirizzo `a1`. Può bloccare il processo.                              |
| **-4**      | `VERHOGEN (V)`     | Esegue un'operazione V non bloccante sul semaforo all'indirizzo `a1`.                                          |
| **-5**      | `DOIO`             | Esegue I/O sincrono scrivendo il comando `a2` all'indirizzo `a1`. Blocca il processo fino fine operazione I/O. |
| **-6**      | `GETCPUTIME`       | Restituisce in `a0` il tempo CPU accumulato (in microsecondi) dal processo chiamante.                          |
| **-7**      | `WAITFORCLOCK`     | Blocca il processo sulla Pseudo-clock finché non viene sbloccato dal Pseudo-clock tick (ogni 100ms).           |
| **-8**      | `GETSUPPORTPTR`    | Restituisce `p_supportStruct` del processo corrente.                                                           |
| **-9**      | `GETPID`           | Restituisce il PID del chiamante (se `a1` è 0) o il PID del genitore.                                          |
| **-10**     | `YIELD`            | Il processo cede volontariamente la CPU.                                                                       |


### Note
- **Incremento del PC**: Bisogna sempre incrementare il valore del PC di 4 per i processi che non vengono terminati, evitando loop infinito di SYSCALL.
- **Stato del processore**: Lo stato del processo salvato nel PCB è **obsoleto** quindi nelle SYSCALL bloccanti è necessario l'aggiornamento con quello salvato nel BIOS Data Page. Naturalmente se si deve ritornare al chiamante bisogna usare lo stato alla chiamata.
- **Valori di ritorno**: Eventuali valori di ritorno vengono posizionati nel registro `a0`.
**Gestione SYSCALL >= 1**: Le chiamate con valore positivo in `a0` non sono gestite direttamente come servizi del Nucleus, ma seguono la politica **Pass Up or Die** verso il livello Support


## Calcolo indice del semaforo per i dispositivi esterni dato indirizzo del command field

Per il calcolo servono informazioni preliminari:
  
| Elemento                                      | Dimensione (Byte) |
| :-------------------------------------------- | :---------------- |
| Un campo del Device Register                  | 0x04              |
| Device Register                               | 0x10              |
| Tutti i Device Register per un interrupt Line | 0x80              |

Abbiamo la formula per i dispositivi non terminal e terminal receiver:

`commandAddr = 0x10000054 + ((IntlineNo - 3) * 0x80) + (DevNo * 0x10) + 0x04` 

per i dispositivi terminal transmitter:

`commandAddr = 0x10000054 + ((IntlineNo - 3) * 0x80) + (DevNo * 0x10) + 0x0C` 

DevNo varia tra 0-7.

### Calcolo IntLineNo
Nella tabella abbiamo riportato che la dimensione totale di tutti i Device Register per un singolo interrupt line è di **0x80**, e la seconda parte (quella che indica il numero del dispositivo e il campo all'interno del registro) non può mai arrivare a 0x80. In base a tale ragionamento si può fare:

`IntlineNo = (commandAddr - 0x10000054) / 0x80 + 3`

Andando a troncare il risultato ottenuto si ottiene `IntlineNo`.

### Calcolo DevNo
Con ragionamente simile al precedente (gli offset 0x04 e 0x08 sono minori della dimensione del Device Register 0x10), sempre con troncamento:

`DevNo = ((commandAddr - 0x10000054) % 0x80) / 0x10`

### Distinguere terminal receiver da transmitter
Basta verificare indirizzo del comando sia riferito al campo 1 (rx) o campo 3 (tx). Contando i campi da 0. Ricordando che un campo ha dimensione 0x04.

Quindi:
- è rx se: `(commandAddr - 0x10000054) % 0x10 == 0x04`
- è tx se: `(commandAddr - 0x10000054) % 0x10 == 0x0C`

Infine con tutti i dati calcolati, e riferendoci alla tabella di assegnazione degli indici otteniamo la formula:

`semIndex = ((IntlineNo - 3) * 8) + (isRx * 8) + DevNo + 1`


## Ricerca e terminazione processo con PID specificato

Sia la ricerca che la terminazione del processo utilizzano la ricorsione, scelta dovuta sia anche al fatto che `MAXPROC` è limitato a 20, limitando la profondità dell'albero, ma anche per una maggiore leggibilità. Se il numero di processi istanziabili fosse maggiore, l'utilizzo della ricorsione sarebbe inaccetabile e si potrebbe usare tecniche come DFS, sfruttando i puntatori `p_parent, p_child, p_sib` (si scende fino alla foglia per poi risalire). Se aumentasse molto forse risulterebbe meglio usare altre strutture dati di supporto come le hash table.


## Interrupts
La gestione segue una gerarchia di priorità: le linee con numero inferiore hanno la precedenza e, all'interno della stessa linea, il dispositivo con numero identificativo più basso viene servito per primo. Viene gestito un interrupt alla volta, se ci sono più richieste pendenti verrà rieseguito il processo per la gestione dell'interrupt.

| Linea | ExcCode | Sorgente           | Descrizione                             |
| :---- | :------ | :----------------- | :-------------------------------------- |
| 1     | `7`     | **PLT**            | Processor Local Timer (Time Slice)      |
| 2     | `3`     | **Interval Timer** | Pseudo-clock (Tick 100ms)               |
| 3-7   | `17-21` | **I/O Devices**    | Disk, Flash, Network, Printer, Terminal |

### Processor Local Timer (PLT)
Viene usato per la **preemption**, quando scatta tale timer (ogni 5ms) il processo in esecuzione viene sospeso per poter dare la possibilità agli altri di usare la CPU.
ACK avviene ricaricando il PLT.

### Interval Timer (Pseudo-clock)
Scatta ogni 100ms e sblocca tutti i dispositivi che aveva fatto la richiesta di bloccarsi fino al prossimo Pseudo-clock tick.
ACK avviene ricaricando il Interval Timer.


### Dispositivi di I/O
Per identificare il dispositivo specifico della classe, il Nucleus consulta la **Interrupting Devices Bit Map**.
Per tali Interrupt è necessario restituire al processo che ha fatto richiesta dell'operazione lo status code finale (in **a0**), per i dispositivi terminal conterrà anche il carattere trasmesso/ricevuto.

Per controllare se era stato la Terminal Transmitter (lo si controlla perché ha priorità maggiore) ad aver generato l'interrupt si ha usato metodo generale: se lo stato non è `UNINSTALLED` o `READY` o `BUSY`, il trasmettitore deve aver completato un'operazione.

ACK avviene mettendo il commando ACK per il dispositivo gestito.


## Accumulo CPU time
Il tempo trascorso dal momento in cui il processo viene  caricato sul processore fino a quando lo lascia volontariamente (tramite una SYSCALL) o involontariamente (per fine del time-slice) viene accreditato al processo stesso. Sono **considerate** anche le SYSCALL in quanto è un servizio chiesto in modo esplicito per propri scopi.

Invece il tempo richiesto per gli interrupt (dispositivi I/O, timer) sono considerati tempi dovuti al Nucleo stesso.


## Pass Up or Die
La politica "Pass Up or Die" definisce il comportamento del Nucleus quando si verifica un'eccezione che non può essere gestita e dev'essere passata al Livello Supporto.

Quando si verifica un'eccezione (TLB, Program Trap o SYSCALL maggiore di 1 o non permessa), il Nucleus controlla se il processo corrente ha definito una struttura per la gestione di tali eventi:
1. **Pass Up**: Se il processo ha `p_supportStruct` **non NULL** si passa il controllo alla funzione di gestione del livello superiore.
2. **Die**: Se non è stata definita la routine di livello superiore, il processo e tutta la sua progenie.
