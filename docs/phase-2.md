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


## Ricerca processo con PId specificato

Anche se la ricerca con `rootProcess` dovrebbe utilizzare la chiamata ricorsiva in quanto non si riesce ad avere memoria dinamica per ad esempio una pila se si vuole evitare la ricorsione.