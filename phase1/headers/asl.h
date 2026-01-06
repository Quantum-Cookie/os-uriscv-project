// SPDX-FileCopyrightText: 2022 Luca Bassi, Gaia Clerici, Mirco Dondi, Fabio Gaiba
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ASL_H_INCLUDED
#define ASL_H_INCLUDED

#include "../../headers/listx.h"
#include "../../headers/types.h"

// Initialize the lists "semdFree_h" and "semd_h" and add all the elements of "semd_table" to "semdFree_h"
/**
 * @brief Inizializza la Active Semaphore List (ASL) e la lista dei semafori liberi.
 * 
 * @return * void : nessun valore di ritorno
 */
void initASL();

// Add the PCB "p" to the semaphore with key "semAdd"
/**
 * @brief Inserisce un PCB nella coda del semaforo specificato.
 * 
 * @param semAdd indirizzo del semaforo nel quale inserire il PCB
 * @param p PCB da inserire nella coda del semaforo
 * @return * int : 0 se l'inserimento è avvenuto con successo, 1 se non ci sono semafori disponibili
 */
int insertBlocked(int* semAdd, pcb_t* p);

// Remove the first PCB from the semaphore with key "semAdd"
/**
 * @brief rimuove e restituisce il primo PCB dalla coda del semaforo specificato.
 * 
 * @param semAdd indirizzo del semaforo dal quale rimuovere il PCB
 * @return * pcb_t* : puntatore al PCB rimosso, NULL se il semaforo non esiste o la coda è vuota
 */
pcb_t* removeBlocked(int* semAdd);

// Remove PCB "p" from its semaphore
/**
 * @brief rimuove un PCB specifico dalla coda del semaforo al quale è bloccato.
 * 
 * @param p PCB da rimuovere dalla coda del semaforo
 * @return * pcb_t* : puntatore al PCB rimosso, NULL se il PCB non è stato trovato
 */
pcb_t* outBlocked(pcb_t* p);

// Return the first blocked PCB of the semaphore with key "semAdd"
/**
 * @brief restituisce il primo PCB dalla coda del semaforo specificato.
 * 
 * @param semAdd indirizzo del semaforo da cui ottenere il PCB
 * @return * pcb_t* : puntatore al PCB in testa alla coda, NULL se il semaforo non esiste o la coda è vuota
 */
pcb_t* headBlocked(int* semAdd);

#endif
