#include "./headers/asl.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

void initASL()
{
    // inizializzazione della lista dei semafori liberi
    INIT_LIST_HEAD(&semdFree_h);

    // addizione di tutti i semafori alla lista vuota
    for (int i = 0; i < MAXPROC; i++)
    {
        list_add(&semd_table[i].s_link, &semdFree_h);
    }
}

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
        // interruzione dell'iterazione se si supera il semaforo cercato
        if (temp->s_key > semAdd)
            break;
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
    list_add(&newSem->s_link, current);

    return 0;
}

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

            // controllo se la coda dei processi è vuota
            if (list_empty(&temp->s_procq))
            {
                return NULL;
            }

            struct list_head *firstPcbL = temp->s_procq.next;
            pcb_t *p = container_of(firstPcbL, pcb_t, p_list);

            // rimozione del PCB dalla coda dei processi
            list_del(firstPcbL);
            p->p_semAdd = NULL;

            // caso lista vuota dopo la rimozione
            if (list_empty(&temp->s_procq))
            {
                // rimozione del semaforo da ASL
                list_del(&temp->s_link);

                // ricollocamento del semaforo nella lista dei semafori liberi
                list_add(&temp->s_link, &semdFree_h);
            }

            return p;
        }

        // interruzione dell'iterazione se si supera il semaforo cercato
        if (temp->s_key > semAdd)
            break;
    }

    return NULL;
}

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

        // interruzione dell'iterazione se si supera il semaforo cercato
        if (temp->s_key > semAdd)
            break;
    }

    return NULL;
}
