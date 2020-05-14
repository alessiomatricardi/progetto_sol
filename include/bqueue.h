/*
 * struttura dati boundered queue, realizzata attraverso array circolare
 * la seguente struttura dati non alloca nè dealloca gli elementi
 * che vi vengono inseriti. Queste devono essere effettuate al di fuori
 * e devono essere indipendenti dalla struttura dati.
*/

#ifndef B_QUEUE_H
#define B_QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <util.h>

typedef struct bqueue_s {
    void** buffer; /* array di puntatori a void */
    size_t max_size; /* numero massimo di elementi nel buffer, da definire nell'inizializzazione */
    size_t head; /* testa della coda, per le estrazioni */
    size_t tail; /* fine della coda, per gli inserimenti */
    unsigned long count; /* numero di elementi attualmente presenti nel buffer */
    pthread_mutex_t mutex; /* variabile di mutua esclusione sulla coda */
    pthread_cond_t cond_empty; /* variabile di condizione per la coda vuota */
    pthread_cond_t cond_full; /* variabile di condizione per la coda piena */
} BQueue_t;

/**
 * alloca e inizializza la coda
 * deve essere chiamata da un solo thread (tipicamente main)
 * 
 * @param num grandezza totale della coda
 * @return NULL se si sono verificati errori nella fase
 * di allocazione o inizializzazione, setta errno
 * @return puntatore alla coda allocata
 */
BQueue_t* init_BQueue(size_t num);

/**
 * dealloca e deinizializza una coda precedentemente allocata con initQueue
 * deve essere chiamata da un solo thread (tipicamente il main)
 *
 * @param q puntatore alla coda da cancellare
 * @param fun funzione di deallocazione del dato presente nella coda
*/
void delete_BQueue(BQueue_t *q, void (*fun)(void*));

/**
 * inserisce un elemento nella coda
 * 
 * @param q puntatore alla coda
 * @param data dato da inserire
 * 
 * @return 0 sul successo
 * @return -1 se fallimento (dato da inserire NULL, errore con mutex, condizioni)
*/
int push(BQueue_t *q, void* data);

/**
 * estrae un elemento dalla coda
 * 
 * @param q puntatore alla coda
 * @return puntatore al dato da restituire
*/
void* pop(BQueue_t *q);


#endif /* B_QUEUE_H */