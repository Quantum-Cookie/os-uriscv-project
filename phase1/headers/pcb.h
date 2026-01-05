// SPDX-FileCopyrightText: 2022 Luca Bassi, Gaia Clerici, Mirco Dondi, Fabio Gaiba
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PCB_H_INCLUDED
#define PCB_H_INCLUDED

#include "../../headers/listx.h"
#include "../../headers/types.h"

// Initialize "pcbFree_h" and add elements of "pcbFree_table" to the list "pcbFree_h"
void initPcbs();

// Add PCB "p" to the list "pcbFree_h"
void freePcb(pcb_t* p);

// Allocate new PCB removing one from list "pcbFree_h" if possible
pcb_t* allocPcb();

// Create an empty PCB list
void mkEmptyProcQ(struct list_head* head);

// Check if the PCB list "head" is empty
int emptyProcQ(struct list_head* head);

// Insert PCB "p" in the list "head"
/**
 * @brief Inserisce il PCB puntato da p nella coda dei processi puntato da head
 * mantenendo la coda ordinata per priorita' decrescente.
 * Se ci fossero piu' PCB con la stessa priorita' p va inserito dopo di essi.
 * 
 * @param head Elemento sentinella della coda dei processi
 * @param p Puntatore al PCB da inserire
 * 
 * @note @p head e @p p devono essere puntatori validi (non NULL)
 */
void insertProcQ(struct list_head* head, pcb_t* p);

// Return the first PCB in the list "head" without removing it
pcb_t* headProcQ(struct list_head* head);

// Remove and return the first PCB in the list "head"
/**
 * @brief Rimuove e restituisce il primo PCB nella lista puntato da @p head
 * 
 * @param head Elemento sentinella della coda dei processi
 * @return `pcb_t*` Puntatore al primo PCB rimosso, o NULL se la coda era vuota
 * 
 * @note @p head deve essere un puntatore valido (non NULL)
 */
pcb_t* removeProcQ(struct list_head* head);

// Remove PCB "p" from the list "head"
/**
 * @brief Rimuove il PCB puntato da @p p dalla coda dei processi puntato da @p head
 * 
 * @param head Elemento sentinella della coda dei processi
 * @param p Puntatore al PCB da rimuovere dalla coda
 * @return `pcb_t*` Puntatore al PCB rimosso, o NULL se il PCB puntato da @p p
 * non appartiene alla lista @p head 
 * 
 * @note @p head e @p p devono essere puntatori validi (non NULL)
 */
pcb_t* outProcQ(struct list_head* head, pcb_t* p);

// Check if the PCB "p" has children
/**
 * @brief Controlla se il PCB @p p e' privo di figli
 * 
 * @param p PCB di cui controllare se ha figli
 * @return int `1` (True) se non ha figli, `0` (False) se ha figli
 * 
 * @note @p p deve essere un puntatore valido (non NULL)
 */
int emptyChild(pcb_t* p);

// Insert the PCB child "p" into the PCB parent "prnt"
/**
 * @brief Rende il PCB puntato @p p figlio del PCB puntato da @p prnt 
 * 
 * @param prnt Puntatore al PCB padre
 * @param p Puntatore PCB figlio da inserire
 * 
 * @note @p prnt e @p p devono essere puntatori validi (non NULL)
 */
void insertChild(pcb_t* prnt, pcb_t* p);

// Remove and return the first child of the PCB "p"
/**
 * @brief Rimuove e restituisce il primo figlio del PCB puntato da @p p
 * 
 * @param p Puntatore al PCB da cui rimuovere il primo figlio
 * @return `pcb_t*` Puntatore al figlio rimosso, o NULL se non ha figli
 * 
 * @note Il campo `p_parent` del figlio rimosso diventa NULL
 * 
 * @note @p p deve essere un puntatore valido (non NULL)
 */
pcb_t* removeChild(pcb_t* p);

// Remove and return the PCB "p" from the parent's children list
/**
 * @brief Rimuove il PCB puntato da @p p dalla lista dei figli del padre (se presente)
 * 
 * @param p Puntatore del PCB di cui bisogna rimuovere la relazione di parentela
 * @return `pcb_t*` @p p, o NULL se non ha padre 
 * 
 * @note Il campo `p_parent` di @p p diventa NULL
 * 
 * @note @p p deve essere un puntatore valido (non NULL)
 */
pcb_t* outChild(pcb_t* p);

#endif
