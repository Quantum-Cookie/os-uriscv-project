# PandOSsh - Fase 1

## Struttura
La fase 1 del progetto si suddivide in 2 moduli principali:

- **PCB**: 
    - Allocazione e la deallocazione dei PCB
    - Gestione della coda dei PCB
    - Gestione degli alberi PCB
- **ASL** 

## Elemento Sentinella
L'elemento sentinella rappresenta una tecnica per poter gestire in modo più "pulito" le varie liste doppie circolari che verranno utilizzate. 

L'elemento sentinella generalmente è semplicemente una struttura `list_head`:

```c
struct list_head {
    struct list_head *next, *prev;
};
``` 

Naturalmente, essendo la testa di una lista doppiamente circolare, il suo puntatore `prev` punta all'ultimo elemento, e `next` punta al primo elemento.

1. Con tale elemento è possibile evitare il controllo sul fatto se l'elemento testa passato alle varie funzioni sia equivalente a `NULL` o meno, ma controllando semplicemente con
```c
    if (head->next == head)
```

2. Permette di poter inserire un elemento sia in testa che in coda senza la necessità di effettuare ulteriori controlli, in quanto si è sicuri che l'elemento head sia sempre diverso da `NULL`. 

3. Permette di rimuovere gli elementi dalla lista puntata da esso senza la necessità di controllare che esso non risulti essere l'unico rimasto e quindi toglie la necessità di aggiornare il valore di `head` (naturalmente i suoi campi `prev` e `next` comunque devono essere aggiornati per essere coerenti con lo stato reale), fornendo punto di accesso fisso alla lista.

## NOTA IMPORTANTE E FILOSOFIA DI PROGETTAZIONE
Nelle funzioni implementate non sono state aggiunte degli controlli sull'effettiva validità dei puntatori (es. `if (head == NULL)`) passati come argomento alle funzioni. La scelta scegue la filosofia `fail-fast`. Si riporta in seguito motivazioni e ragionamenti:

- **Efficienza:** Essendo un implementazione al livello 2 (Queue Manager), sulla quale si basa tutto, formando il nucleo del sistema. Evitando controlli si massimizza le prestazioni a livello globale.
- **Debugging:** Il passaggio di un puntatore nullo a questo livello è considerato un errore logico del chiamante. Gestirlo silenziosamente potrebbe provocare errori altrove rendendo più difficile il debug. 
- **Strategia di sviluppo:** Durante la fase di sviluppo è possibile gestire tale errore e affinare con un log ad esempio nel `klog_buffer`, ed entrare in un loop infinito per avere la possibilità di identificarlo.

## PCB
I PCB (Process Control Block) sono la rappresentazione dei processi attraverso l'utilizzo di una struttura dati. La parte fondamentale che li rende interconnessi ad un livello di astrazione maggiore (Process Queue, Process Tree) sarebbe l'utilizzo della struttura dati `list_head`.

Infatti sono presenti in diversi campi:

- `p_list` serve per collegare il PCB alla lista dei processi liberi, oppure alla coda dei processi attivi

- `p_child` rappresenterebbe l'elemento sentinella della lista dei figli

- `p_sib` serve per collegarlo alla lista dei suoi fratelli 

## Architettura della Memoria
Il sistema PandOSsh adotta una strategia di **allocazione statica** per la gestione dei *Process Control Blocks* (PCB). In questo modo si può elimina la necessità di gestire la memoria dinamica durante l'esecuzione del kernel, riducendo il rischio di frammentazione e garantendo tempi di esecuzione deterministici.

### Componenti Principali
- **pcbFree_table:** Array statico di dimensione `MAXPROC` che funge da pool fisico di memoria.
- **pcbFree_h:** Sentinella (list_head) della lista dei processi liberi.
- **next_pid:** Contatore intero per l'assegnazione di identificativi univoci ai processi.

## Allocazione deallocazione PCB
### Inizializzazione (`initPcbs`)
La funzione `initPcbs` prepara il sistema durante la fase di avvio. Inizializza la sentinella `pcbFree_h` e inserisce tutti i PCB contenuti nell'array statico all'interno della lista dei liberi. Ogni elemento viene predisposto tramite la macro `INIT_LIST_HEAD`.

### Allocazione (`allocPcb`)
Questa funzione permette di prelevare un PCB dal pool per attivare un nuovo processo.
    1. **Rimozione:** Estrae il primo elemento disponibile dalla lista `pcbFree_h`. Se la lista è vuota, restituisce `NULL`.
    2. **Reset:** Tutti i campi della struttura vengono azzerati (priorità, tempi, puntatori ai semafori) per garantire che il nuovo processo non erediti dati "sporchi".
    3. **Identificazione:** Assegna un PID univoco incrementando `next_pid`. In caso di overflow del contatore, il sistema lo resetta automaticamente a 1.

### Deallocazione (`freePcb`)
Utilizzata quando un processo termina. La funzione esegue un "deep reset" del PCB (azzerando relazioni parentali e priorità) e lo reinserisce nella coda dei liberi, rendendolo disponibile per future allocazioni.


## Scelte implementative
- È stato introdotto una nuova funzione:
```c
static struct list_head *listRemoveFirst(struct list_head *head)
```
In quanto si è notata la parziale duplicazione del codice tra la funzione `removeProcQ` e `removeChild` in quanto rimuovevano sempre il primo elemento della lista per effettuare poi altre operazioni.
Tale funzione è statica in quanto è necessario solo all'interno di `pcb.c`

## Gestione della coda dei processi
La coda dei processi (implementata tramite una lista doppiamente concatenata circolare), è gestita come coda con priorità. Mantenute in ordine decrescente di priorità memorizzato in `p_prio`.  
Il modulo fornisce le seguenti operazioni fondamentali:

- `mkEmptyProcQ`: Inizializza una testa della lista (sentinella) come vuota.

- `emptyProcQ`: Verifica se la lista contiene elementi (`head→next==head`).

- `headProcQ`: Restituisce il puntatore al primo PCB della lista senza rimuoverlo.

**Analisi delle prestazioni**:
- `removeProcQ`: La rimozione dell'elemento con priorità maggiore con costo `O(1)` in qualunque caso.

- `insertProcQ`: Tuttavia la necessità di mantenerlo ordinato provoca un aumento di costo computazionale per l'inserimento nel caso pessimo, il quale risulta essere `O(n)`. Inoltre viene garantita la stabilità dell'inserimento, ovvero il nuovo processo viene inserito dopo quelli già presenti con la stessa priorità.

- `outProcQ`: Il costo della rimozione di un PCB specificato è semplicemente il costo della ricerca in una lista che nel caso pessimo è `O(n)`. 

### Analisi costo per inserimento
In fase di progettazione, abbiamo ipotizzato soluzioni per ottimizzare l'inserimento cercando di ridurre il costo del caso pessimo, ma sono state scartate per i seguenti motivi:

1. **Array di liste**: L'idea di utilizzare un array dove ogni indice corrisponde a un livello di priorità avrebbe garantito accessi `O(1)`. Tuttavia, i file di configurazione forniti non definiscono un valore massimo per la priorità, rendendo impossibile un'allocazione statica dell'array. Inoltre, con priorità sparse, si verificherebbe un inutile spreco di memoria.

2. **Tabelle Hash**: L'utilizzo di una hashmap è stato escluso poiché la gestione delle collisioni (tramite concatenamento o indirizzamento aperto) non eliminerebbe la necessità di scansioni lineari, introducendo al contempo un overhead di memoria e una maggiore complessità logica.

**Conclusione:** Considerando che il numero massimo di processi gestiti è limitato a 20 (`MAXPROC`), l'overhead causato dalla scansione `O(n)` è del tutto trascurabile. La scelta di una lista ordinata risulta quindi la più efficiente in termini di rapporto tra semplicità del codice, utilizzo di memoria e prestazioni reali.

### Scelte implementative discutibili
- `insertProcQ`: è stato deciso di utilizzare la funzione `__list_add` in modo che fosse chiaro il punto di inserimento. L'alternativa sarebbe quello di usare `list_add_tail`, il quale farebbe la stessa cosa, però risulta meno leggibile il codice. 

### Possibile utilizzo della Process Queue
Un esempio banale dell'utilizzo della Process Queue può essere quello per aiutare la scelta dello scheduler di quali processi mandare in esecuzione dalla coda ready. Naturalmente negli scheduler veri bisogna anche considerare per quanto tempo un processo non è stato eseguito e aumentargli priorità, quindi si potrebbe fare  con una rimozione e un reinserimento dopo l'aumento della priorità.  

## Gestione dell'albero dei processi
L'albero dei processi sarebbe una struttura che permette di gestire le varie relazioni presenti tra i PCB.

### Relazioni presenti
- `p_parent`: è un puntatore diretto al padre.
- `p_child`: rappresenta elemento sentinella della lista dei propri figli.
- `p_sib`: serve per permettere al PCB di fare parte della lista dei fratelli dello stesso padre.

### Vantaggio dell'organizzazione
Nell'ambito degli alberi emerge chiaramente il vantaggio di utilizzare una lista doppiamente concatenata circolare. Esso fornisce la possibilità di avere tutte le operazioni sull'albero dei processi (`emptyChild`, `insertChild`, `removeChild`, `outChild`) con costo `O(1)`.

- `emptyChild`: un semplice controllo per vedere se `p_child` sia vuota o meno
- `insertChild`: l'inserimento di un elemento in una lista doppiamente concatenata circolare sia all'inizio che alla fine ha costo costante. Nell'implementazione si è deciso di mettere alla fine della lista dei figli `p_child`.
- `removeChild`: basta rimuovere il primo figlio (se esiste) dalla lista `p_child` e aggiornare il campo `p_sib` del figlio.
- `outChild`: in quanto viene passato direttamente il PCB da togliere, risulta sufficiente aggiornare il campo padre `p_parent` a `NULL` e togliere il collegamento tra i fratelli ottenuto con `p_sib`.

### Possibile utilizzo del Process Tree
Un esempio dell'utilizzo del Process Tree può essere la terminazione a cascata. Generalmente quando un processo padre viene terminato, il kernel deve identificare e far terminare anche tutti i suoi figli. 