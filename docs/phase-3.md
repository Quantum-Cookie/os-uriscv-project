# PandOSsh - Fase 3

## Variabili Globali
Sono state introdotte variabili condivise tra i moduli del Support Level:

`int suppIOMutexSemaphores[NSUPPSEM]`: Array unico di semafori di mutua esclusione per l'accesso ai dispositivi I/O da parte del Support Level (uno per ciascuna coppia dispositivo/direzione). L'indice viene calcolato tramite la macro `GET_IO_MUTEX_SEMAPHORE_INDEX`.

| Index       | Device Class               |
| :---------- | :------------------------- |
| **0 – 7** | Disk Devices               |
| **8 – 15** | Flash Devices              |
| **16 – 23** | Network (Ethernet) Devices |
| **24 – 31** | Printer Devices            |
| **32 – 39** | Terminal Devices (TX)      |
| **40 – 47** | Terminal Devices (RX)      |

`int masterSemaphore`: Semaforo su cui si blocca l'**InstantiatorProcess**. Viene sbloccato (V) solo quando termina la Shell, garantendo che il sistema resti attivo finché la Shell è in esecuzione.

`int shellSemaphore`: Semaforo su cui si blocca la Shell dopo aver lanciato un U-proc figlio tramite `SYS6 Execute`, in attesa che quest'ultimo invochi `SYS2 Terminate`.

`memaddr SWAP_POOL_START_ADDR`: Indirizzo fisico di inizio della Swap Pool, calcolato una sola volta all'avvio subito dopo l'area occupata dal Sistema Operativo.


## Costanti
Costanti aggiunte in `const.h` per la Fase 3:

- `SHELL_ASID: 1`: ASID riservato al processo Shell.
- `READTERMINAL: 5` / `EXECUTE: 6`: numeri delle SYSCALL di livello Support (SYS5, SYS6), che si aggiungono a 


## Gestione delle Support Structure
Analogamente a quanto fatto per i PCB in Fase 1, le Support Structure sono allocate staticamente (array `supportStructures[8]`) e gestite tramite una free list (`supportFree_h`), evitando allocazione dinamica.

- `initSupportFreeList`: inizializza la sentinella e inserisce tutte le Support Structure dell'array nella lista dei liberi.
- `suppListRemoveFirst`: estrae il primo elemento della free list, analoga a `listRemoveFirst` usata per i PCB in Fase 1.
- `allocateSupportStructure(asid)`: rimuove una struttura libera e la inizializza per l'ASID richiesto tramite `initSupportStructure`; restituisce `NULL` se non ce ne sono disponibili.
- `deallocateSupportStructure(s)`: reinserisce la struttura nella free list senza pulirne i campi, poiché verranno tutti riscritti dalla successiva `initSupportStructure`.


### Inizializzazione di una Support Structure (`initSupportStructure`)
Oltre a impostare l'ASID, configura:

- il PC dei due contesti di eccezione, puntando rispettivamente a `TLBPagerHandler` (pager) e `generalSupportHandler` (SYSCALL/Program Trap);
- lo Status dei contesti con interrupt abilitati, così che gli handler stessi siano interrompibili;
- lo Stack Pointer dei due contesti, calcolato tramite `tlbStackSP` e `genStackSP`.

Infine legge dal dispositivo Flash associato all'ASID la pagina 0 (contenente l'header aout dell'eseguibile) per determinare la dimensione del segmento `.text` e, di conseguenza, quante pagine della Page Table dovranno essere marcate come Read-Only. La lettura è protetta sia dal semaforo del dispositivo Flash sia da un semaforo dedicato al buffer condiviso `uprocHeader`, poiché quest'ultimo è comune a tutte le inizializzazioni.

### Stack degli handler del Support Level
Lo stack di `InstantiatorProcess()` occupa l'ultimo frame di RAM (`ramTop - PAGESIZE`, fino a `ramTop`). Subito sotto tale frame (`supportStacksBase`) vengono ricavati, per ciascun ASID, due frame dedicati:

- il primo (più in alto) per lo stack del gestore TLB (`tlbStackSP`);
- il secondo (più in basso) per lo stack del gestore generale (`genStackSP`).


### Inizializzazione della Page Table (`initPageTable`)
Per ogni U-proc viene costruita una Page Table di 32 entry:

- le prime 31 entry corrispondono alle pagine `.text`/`.data`, con VPN progressivo da `0x80000`;
- la 32-esima entry rappresenta l'unica pagina di stack, con VPN fisso `0xBFFFF`.

In `EntryHI` viene inserito anche l'ASID del processo. In `EntryLO` tutte le pagine sono inizialmente invalide (nessun frame associato); il bit Dirty viene comunque preimpostato per distinguere le pagine di sola lettura (`.text`, Dirty = 0) da quelle scrivibili (`.data` e stack, Dirty = `DIRTYON`), sulla base del numero di pagine di testo (`textPages`) ricavato dall'header aout.

## Swap Pool (`vmSupport.c`)
La Swap Pool è l'area di RAM condivisa in cui vengono caricate le pagine logiche degli U-proc. È descritta da una tabella di dimensione `SWAP_POOL_SIZE = 2 * UPROCMAX` (2 frame per ogni U-proc), il cui accesso è protetto dal semaforo `swapPoolSemaphore`.

Ogni entry della tabella (`swap_t`) memorizza l'ASID del proprietario, il numero di pagina logica ospitata e un puntatore alla relativa entry della Page Table, così da poter risalire e invalidare la pagina in caso di rimpiazzamento.

- `initSwapStructs`: inizializza il semaforo a 1 e marca tutti i frame come liberi (`sw_asid = NOPROC`).
- `replacementAlgorithm`: cerca innanzitutto un frame libero; se non esiste, sceglie la vittima con una politica Round Robin sull'intero pool. Deve essere invocata all'interno della sezione critica protetta da `swapPoolSemaphore`.
- `releaseFrames(asid)`: scandisce la tabella e libera tutti i frame appartenenti all'ASID indicato (usata da `SYS2 Terminate`), invalidando atomicamente anche l'eventuale entry corrispondente nella TLB.

### Pager (`TLBPagerHandler`)
Gestisce le eccezioni TLB-Invalid (load e store, non distinte esplicitamente, semplicemete si va a vedere il dirty bit) generate dagli U-proc.

#### Indirizzo e comando per l'I/O sul Flash

Ogni operazione sul dispositivo Flash (sia in lettura che in scrittura) richiede di impostare due registri del device register (`dtpreg_t`):

- `data0`: è l'indirizzo fisico in RAM verso cui (in lettura) o da cui (in scrittura) verrà trasferito il contenuto della pagina. Corrisponde sempre all'indirizzo del frame della Swap Pool coinvolto, calcolato come SWAP_POOL_START_ADDR + (victimFrame * PAGESIZE)

- `command`: contiene sia l'operazione da eseguire sia il blocco (pagina) del Flash da leggere/scrivere. Il byte più basso contiene il codice operazione (`FLASHREAD` o `FLASHWRITE`), mentre il numero di blocco viene scritto negli 8 bit successivi tramite uno shift di 8 posizioni: `command = numeroBlocco << 8 | operazione`.

Nel caso della scrittura (salvataggio del frame vittima), il numero di blocco è la pagina logica che il frame vittima ospitava (`swapPoolTable[victimFrame].sw_pageNo`), perché la pagina va riscritta esattamente nella posizione del Flash da cui era stata originariamente caricata. Nel caso della lettura, il numero di blocco è invece `vpnMissed`, cioè la pagina logica mancante che ha causato il page fault, poiché è quella che deve essere recuperata dal Flash dell'ASID corrente.

In ogni punto in cui un'operazione di I/O sul Flash fallisce, viene invocato `programTrapHandler` (dopo aver rilasciato i semafori acquisiti), che a sua volta termina il processo come `SYS2 Terminate`.

L'aggiornamento di Page Table e TLB deve essere **atomico** (interrupt disabilitati) perché sono due copie della stessa informazione: se un interrupt (es. PLT) causasse un cambio di contesto tra l'aggiornamento dell'una e dell'altra, si potrebbe eseguire con una TLB non coerente con la Page Table, portando a un comportamento indefinito.

## Gestione delle eccezioni del Support Level (`sysSupport.c`)
`generalSupportHandler` riceve dal Nucleo (via Pass Up or Die) tutte le eccezioni non-TLB indirizzate al Support Level e smista in base all'exception code:

- codice `8` o `11` (SYSCALL): delegata a `syscallHandler`;
- qualunque altro codice (Program Trap): delegata a `programTrapHandler`.

`syscallHandler` incrementa il PC di 4 (per evitare loop sulla SYSCALL) e smista in base al valore di `a0`:

| Valore (a0)      | Nome Servizio   | Descrizione                                                        |
| :--------------- | :-------------- | :------------------------------------------------------------------ |
| **`TERMINATE`**    | SYS2 Terminate  | Termina l'U-proc chiamante e rilascia le sue risorse.               |
| **`WRITETERMINAL`**| SYS4 WriteTerminal | Scrive una stringa sul Terminal 0.                               |
| **`READTERMINAL`** | SYS5 ReadTerminal  | Legge una riga (fino a `\n`) dal Terminal 0.                     |
| **`EXECUTE`**      | SYS6 Execute       | Istanzia un nuovo U-proc con l'ASID specificato.                 |

Al termine dello smistamento (per le SYSCALL che ritornano al chiamante) viene eseguita `LDST` per ripristinare lo stato del processore.

### Validazione degli indirizzi utente (`isValidAddress`, `isValidArea`)
Le SYSCALL che ricevono un puntatore a buffer dall'U-proc (`SYS4`, `SYS5`) devono verificare che gli indirizzi coinvolti siano leciti, per evitare che un U-proc acceda a memoria non sua:

- `isValidAddress`: un indirizzo è valido se ricade nell'area `.text`/`.data` (`0x80000000` - `0x8001E000`, esclusa) oppure nell'unica pagina di stack (`0xBFFFF000` - `0xC0000000`, esclusa).
- `isValidArea`: verifica inoltre che un intero intervallo (buffer) sia contenuto **interamente** in una sola delle due aree, impedendo che un buffer "scavalchi" da un'area logica all'altra.

In caso di violazione, la SYSCALL invoca direttamente `terminate` sull'U-proc.

### SYS2 Terminate
1. Rilascia tutti i frame della Swap Pool occupati dall'ASID del chiamante (`releaseFrames`).
2. Se il chiamante è la Shell (`SHELL_ASID`), effettua una V su `masterSemaphore`, sbloccando l'`InstantiatorProcess` e avviando lo spegnimento del sistema; altrimenti effettua una V su `shellSemaphore`, risvegliando la Shell in attesa.
3. Restituisce la Support Structure alla free list (`deallocateSupportStructure`).
4. Termina effettivamente il processo a livello di Nucleo tramite `SYSCALL(TERMPROCESS, ...)`.

`programTrapHandler` si limita a recuperare la Support Structure corrente e a invocare `terminate`, trattando quindi un Program Trap come una richiesta implicita di terminazione.

### SYS4 WriteTerminal / SYS5 ReadTerminal
Entrambe le SYSCALL operano sul Terminal 0 e usano un semaforo dedicato di `suppIOMutexSemaphores` (indici differenti per trasmissione e ricezione, calcolati con `GET_IO_MUTEX_SEMAPHORE_INDEX`) per garantirne l'uso in mutua esclusione tra U-proc.

- **WriteTerminal**: verifica che la lunghezza richiesta sia compresa tra 0 e 128 caratteri e che l'intero buffer sia in un'area valida, quindi trasmette un carattere alla volta (comando `PRINTCHR`). In caso di errore di trasmissione, restituisce in `a0` il valore negativo dello status; in caso di successo, restituisce il numero di caratteri trasmessi.
- **ReadTerminal**: legge un carattere alla volta (comando `RECEIVECHAR`) finché non viene ricevuto `\n` (incluso nel conteggio), verificando ad ogni iterazione che l'indirizzo di scrittura sia ancora valido e appartenente alla stessa area logica di partenza. Restituisce il numero di caratteri ricevuti, oppure il valore negativo dello status in caso di errore.

### SYS6 Execute
Crea un nuovo Uproc in base all'ASID passato come argomento, dev'essere compreso tra 1 e 8. In quanto in tale progetto può essere invocato dalla Shell, si effettua una P su `shellSemaphore` in attesa che l'U-proc invocato termini con `SYS2 Terminate`.


### Calcolo dell'indirizzo di inizio della Swap Pool
L'header dell'eseguibile del Sistema Operativo si trova in RAM subito dopo l'area riservata al BIOS. A partire da tale header si estraggono l'indirizzo virtuale e la dimensione del segmento `.data` (ultimo segmento del Kernel), la cui somma (`os_end`) rappresenta il limite superiore del codice occupato dal Sistema Operativo. `SWAP_POOL_START_ADDR` viene quindi ottenuto arrotondando per eccesso `os_end` all'inizio della pagina fisica successiva, così da non sovrapporsi mai al codice o ai dati del Kernel.

## Scelte implementative
- La mappatura tra ASID e relativo dispositivo Flash è semplicemente `Flash devNo = ASID - 1` (gli ASID vanno da 1 a 8, mentre i device number da 0 a 7).

- Le Support Structure, come i PCB in Fase 1, sono allocate staticamente (array di dimensione `UPROCMAX`) e gestite con una free list.

- La lettura dell'header aout per determinare le pagine `.text` di sola lettura avviene tramite un buffer comune (`uprocHeader`) protetto da un semaforo dedicato, per evitare di allocare un buffer per pagina in ciascuna Support Structure.