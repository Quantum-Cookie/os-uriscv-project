#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

void initPcbs() {
}

void freePcb(pcb_t* p) {
}

pcb_t* allocPcb() {
}

void mkEmptyProcQ(struct list_head* head) {
}

int emptyProcQ(struct list_head* head) {
}

void insertProcQ(struct list_head* head, pcb_t* p) {
    /* Punto di inserimento di default e' alla fine della coda */
    struct list_head *insertPos = head;

    pcb_t *iter;

    /*  Scorre la lista per trovare il primo processo con priorita'
        minore di p, in quanto bisogna mantenere la lista ordinata per 
        priorita' in modo decrescente */
    list_for_each_entry(iter, head, p_list) {
        if (iter->p_prio < p->p_prio) {
            insertPos = &(iter->p_list);
            break;
        }
    }

    /*  Inserisce p immediatamente prima di insertPos.
        Se insertPos e' ancora head alla viene inserito in fondo alla lista */
    __list_add(&(p->p_list), insertPos->prev, insertPos);
}

pcb_t* headProcQ(struct list_head* head) {
}

/**
 *  @brief Estrae il primo elemento della lista puntato da head e pulisce i suoi campi
 *  
 *  @param head Puntatore alla testa della lista da cui rimuovere l'elemento
 * 
 *  @return NULL se la lista e' vuota, altrimenti restituisce il puntatore a elemento rimosso
 */
static struct list_head *listRemoveFirst(struct list_head *head) {
    /* Controllo per vedere se la lista e' vuota o meno */ 
    if (list_empty(head))
        return NULL;

    /* Rimuove primo elemento della lista e pulisce i suoi campi di list_head */
    struct list_head *toRemove = head->next;
    list_del(toRemove);

    return toRemove;
}

pcb_t* removeProcQ(struct list_head* head) {
    /* Estrae il primo nodo dalla coda dei processi */
    struct list_head *removed = listRemoveFirst(head);

    /* Se e' stato correttamente rimosso restituisce il PCB relativo, altrimenti NULL */
    return !removed ? NULL : container_of(removed, pcb_t, p_list);
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {    
    pcb_t *iter;

    /* Scorre la coda dei processi per trovare p */
    list_for_each_entry(iter, head, p_list) {
        /* Appena p e' stato trovato lo rimuove dalla lista e lo restituisce */
        if (iter == p) {
            list_del(&(iter->p_list));
            return iter;
        }
    }

    /* Se p non e' stato trovato restituisce NULL */ 
    return NULL;
}

int emptyChild(pcb_t* p) {
    /* Se la lista p_child è vuota, il processo non ha figli */
    return list_empty(&(p->p_child));
}

void insertChild(pcb_t* prnt, pcb_t* p) {
    /* Inserisce il processo figlio p in fondo alla lista dei figli di prnt */
    list_add_tail(&(p->p_sib), &(prnt->p_child));

    /* Aggiorna il puntatore al padre di p */
    p->p_parent = prnt;
}

pcb_t* removeChild(pcb_t* p) {
    /* Rimuove il primo figlio dalla lista dei figli di p */
    struct list_head *removed = listRemoveFirst(&(p->p_child));

    /* Restituisce NULL se non ci sono figli */
    if (!removed)
        return NULL;

    /* Pulisce il campo padre del figlio rimosso */
    pcb_t *pcbRemoved = container_of(removed, pcb_t, p_sib);
    pcbRemoved->p_parent = NULL;

    return pcbRemoved;
}

pcb_t* outChild(pcb_t* p) {
    /* Controlla se p ha un PCB padre */
    if (!(p->p_parent))
        return NULL;

    /* Rimuove p dalla lista dei figli del padre e pulisce il campo padre di p*/
    list_del(&(p->p_sib));
    p->p_parent = NULL;

    return p;
}
