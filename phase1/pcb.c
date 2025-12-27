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
    /* Puntatore al nodo davanti al quale verra' inserito p */
    struct list_head *insertPos = head;

    pcb_t *iter;

    list_for_each_entry(iter, head, p_list) {
        /* Se la priorita' di iter e' minore di quello di p, allora abbiamo trovato punto di inserimento */
        if (iter->p_prio < p->p_prio) {
            insertPos = &(iter->p_list);
            break;
        }
    }

    __list_add(&(p->p_list), insertPos->prev, insertPos);
}

pcb_t* headProcQ(struct list_head* head) {
}

/* Rimuove il primo elemento della lista puntato da head */
static struct list_head *listRemoveFirst(struct list_head *head) {
    /* Controllo per vedere se la lista e' vuota o meno */ 
    if (list_empty(head))
        return NULL;

    /* Rimuove primo elemento della lista */
    struct list_head *toRemove = head->next;
    list_del(toRemove);

    return toRemove;
}

pcb_t* removeProcQ(struct list_head* head) {
    /* Rimuove il primo PCB dalla coda dei processi */
    struct list_head *removed = listRemoveFirst(head);

    return !removed ? NULL : container_of(removed, pcb_t, p_list);
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {    
    pcb_t *iter;

    list_for_each_entry(iter, head, p_list) {
        /* Se p e' stato trovato nella lista lo rimuove e lo restituisce */ 
        if (iter == p) {
            list_del(&(iter->p_list));
            return iter;
        }
    }

    /* Se p non e' stato trovato restituisce NULL */ 
    return NULL;
}

int emptyChild(pcb_t* p) {
    /* Return controllando se struct list_head p_child sia vuota o meno */ 
    return list_empty(&(p->p_child));
}

void insertChild(pcb_t* prnt, pcb_t* p) {
    /* Aggiunge il processo figlio p in fondo alla lista dei figli di prnt */
    list_add_tail(&(p->p_sib), &(prnt->p_child));

    /* Aggiorna il padre di p */
    p->p_parent = prnt;
}

pcb_t* removeChild(pcb_t* p) {
    /* Rimuove il primo figlio dalla lista dei figli di p */
    struct list_head *removed = listRemoveFirst(&(p->p_child));

    /* Controlla se ci sono figli rimossi */
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

    /* Toglie p dai figli del PCB padre */
    list_del(&(p->p_sib));
    p->p_parent = NULL;

    return p;
}
