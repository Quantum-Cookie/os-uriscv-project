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

pcb_t* removeProcQ(struct list_head* head) {
    /* Controllo per vedere se la lista e' vuota o meno */ 
    if (list_empty(head))
        return NULL;

    /* Rimozione del primo PCB (priorita' piu' alta) */
    struct list_head *toRemove = head->next;
    list_del(toRemove);

    return container_of(toRemove, pcb_t, p_list);
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
}

int emptyChild(pcb_t* p) {
}

void insertChild(pcb_t* prnt, pcb_t* p) {
}

pcb_t* removeChild(pcb_t* p) {
}

pcb_t* outChild(pcb_t* p) {
}
