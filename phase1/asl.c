#include "./headers/asl.h"
#include "./headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

/**
 * @brief Inizializza la Active Semaphore List (ASL) e la lista dei semafori liberi.
 * 
 * @return * void : nessun valore di ritorno
 */

void initASL()
{
    // inizializzazione della lista dei semafori liberi
    INIT_LIST_HEAD(&semdFree_h);

    // inizializzazione della lista dei semafori attivi
    INIT_LIST_HEAD(&semd_h);

    // addizione di tutti i semafori alla lista vuota
    for (int i = 0; i < MAXPROC; i++)
    {
        list_add(&semd_table[i].s_link, &semdFree_h);
    }
}

/**
 * @brief Inserisce un PCB nella coda del semaforo specificato.
 * 
 * @param semAdd indirizzo del semaforo nel quale inserire il PCB
 * @param p PCB da inserire nella coda del semaforo
 * @return * int : 0 se l'inserimento è avvenuto con successo, 1 se non ci sono semafori disponibili
 */

int insertBlocked(int *semAdd, pcb_t *p)
{
    struct list_head *current;

    // iterazione attraverso la lista
    list_for_each(current, &semd_h)
    {
        semd_t *temp = container_of(current, semd_t, s_link);
        if (temp->s_key == semAdd)
        {
            // semaforo trovato, aggiunta del PCB alla coda dei processi
            list_add_tail(&p->p_list, &temp->s_procq);
            p->p_semAdd = semAdd;
            return 0;
        }
    }

    // semaforo non trovato, nessun semaforo disponibile
    if (list_empty(&semdFree_h))
    {
        return 1;
    }

    // estrazione di un semaforo dalla lista dei semafori liberi
    struct list_head *newSemL = semdFree_h.next;
    list_del(newSemL);

    semd_t *newSem = container_of(newSemL, semd_t, s_link);

    // inizializzazione del nuovo semaforo
    newSem->s_key = semAdd;
    mkEmptyProcQ(&newSem->s_procq);

    // inserzione del processo nella coda del nuovo semaforo
    list_add_tail(&p->p_list, &newSem->s_procq);
    p->p_semAdd = semAdd;

    // inserzione del nuovo semaforo nella ASL
    list_add_tail(&newSem->s_link, &semd_h);

    return 0;
}

/**
 * @brief rimuove e restituisce il primo PCB dalla coda del semaforo specificato.
 * 
 * @param semAdd indirizzo del semaforo dal quale rimuovere il PCB
 * @return * pcb_t* : puntatore al PCB rimosso, NULL se il semaforo non esiste o la coda è vuota
 */

pcb_t *removeBlocked(int *semAdd)
{
    struct list_head *current;

    // iterazione attraverso la lista dei semafori
    list_for_each(current, &semd_h)
    {
        semd_t *temp = container_of(current, semd_t, s_link);

        // trovato il semaforo cercato
        if (temp->s_key == semAdd)
        {

            struct list_head *firstPcbL = temp->s_procq.next;
            pcb_t *p = container_of(firstPcbL, pcb_t, p_list);

            // rimozione del PCB dalla coda dei processi
            list_del(firstPcbL);
            p->p_semAdd = NULL;

            // caso lista vuota dopo la rimozione
            if (emptyProcQ(&temp->s_procq))
            {
                // rimozione del semaforo da ASL
                list_del(&temp->s_link);

                // ricollocamento del semaforo nella lista dei semafori liberi
                list_add_tail(&temp->s_link, &semdFree_h);
            }

            return p;
        }
    }

    return NULL;
}

/**
 * @brief rimuove un PCB specifico dalla coda del semaforo al quale è bloccato.
 * 
 * @param p PCB da rimuovere dalla coda del semaforo
 * @return * pcb_t* : puntatore al PCB rimosso, NULL se il PCB non è stato trovato
 */

pcb_t *outBlocked(pcb_t *p)
{
    struct list_head *current;

    // iterazione attraverso la lista dei semafori
    list_for_each(current, &semd_h)
    {
        semd_t *temp = container_of(current, semd_t, s_link);

        // trovato il semaforo cercato
        if (temp->s_key == p->p_semAdd)
        {
            // rimozione del PCB dalla coda dei processi
            list_del(&p->p_list);
            p->p_semAdd = NULL;

            // rimozione del semaforo se la coda è vuota
            if (list_empty(&temp->s_procq))
            {
                list_del(&temp->s_link);
                list_add(&temp->s_link, &semdFree_h);
            }

            return p;
        }

        // interruzione dell'iterazione se si supera il semaforo cercato
        if (temp->s_key > p->p_semAdd)
            break;
    }

    return NULL;
}

/**
 * @brief restituisce il primo PCB dalla coda del semaforo specificato.
 * 
 * @param semAdd indirizzo del semaforo da cui ottenere il PCB
 * @return * pcb_t* : puntatore al PCB in testa alla coda, NULL se il semaforo non esiste o la coda è vuota
 */

pcb_t *headBlocked(int *semAdd)
{
    struct list_head *current;

    // iterazione attraverso la lista dei semafori
    list_for_each(current, &semd_h)
    {
        semd_t *temp = container_of(current, semd_t, s_link);

        // trovato il semaforo cercato
        if (temp->s_key == semAdd)
        {
            if (list_empty(&temp->s_procq))
            {
                return NULL;
            }

            struct list_head *headPcbL = temp->s_procq.next;
            return container_of(headPcbL, pcb_t, p_list);
        }
    }

    return NULL;
}
