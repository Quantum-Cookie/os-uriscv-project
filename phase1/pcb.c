#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

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

void initPcbs() {
    INIT_LIST_HEAD(&pcbFree_h);
    for (int i = 0; i < MAXPROC; i++) {

        /* Inizializzazione dei puntatori della lista interna al PCB 
        e inserimento del PCB corrente nella lista dei PCB liberi */
        INIT_LIST_HEAD(&(pcbFree_table[i].p_list));
        list_add_tail(&(pcbFree_table[i].p_list), &pcbFree_h);
    }
}

void freePcb(pcb_t* p) {
    if (p == NULL) return; /* Controllo di sicurezza sempre utile, con puntatore nullo non si fa nulla */

    /* Reset di tutti i campi del PCB e inizializzazione delle liste */
    p->p_parent = NULL;
    INIT_LIST_HEAD(&(p->p_child));
    INIT_LIST_HEAD(&(p->p_sib));
    p->p_semAdd = NULL;
    p->p_supportStruct = NULL;
    p->p_prio = 0;
    p->p_time = 0;
    p->p_pid = 0;

    /* Reinserimento del PCB nella lista dei PCB liberi */
    list_add_tail(&(p->p_list), &pcbFree_h);
}

pcb_t* allocPcb() {
    /* Estrae un PCB dalla lista dei liberi */
    struct list_head *removed = listRemoveFirst(&pcbFree_h);
    if (!removed)
        return NULL;

    pcb_t *newPcb = container_of(removed, pcb_t, p_list);

    /* Inizializza i campi del PCB */
    newPcb->p_parent = NULL;
    INIT_LIST_HEAD(&(newPcb->p_child));
    INIT_LIST_HEAD(&(newPcb->p_sib));
    newPcb->p_semAdd = NULL;
    newPcb->p_supportStruct = NULL;
    newPcb->p_prio = 0;
    newPcb->p_time = 0;

    /* Serve per assegnare un PID univoco */
    newPcb->p_pid = next_pid++;
    if (next_pid <= 0) /* Per la gestione overflow */
        next_pid = 1;

    return newPcb;
}

void mkEmptyProcQ(struct list_head* head) {
    /*Inizializza una coda di processi come vuota. */
    INIT_LIST_HEAD(head);
}

int emptyProcQ(struct list_head* head) {
    /*Controlla se la coda dei processi è vuota. */
    return list_empty(head);
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
    /* Verifica che la coda sia vuota */
    if (emptyProcQ(head)) {
        return NULL;
    }

    /* Prende il primo elemento della lista */
    struct list_head *first = head->next;

    /* Restituisce il PCB relativo al primo elemento della lista */
    return container_of(first, pcb_t, p_list);
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
