/*
 * struttura dati unboundered queue NON CONCORRENTE,
 * realizzata attraverso lista
 * la seguente struttura dati non alloca nè dealloca gli elementi
 * che vi vengono inseriti. Queste devono essere effettuate al di fuori
 * e devono essere indipendenti dalla struttura dati.
*/

#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

struct queue_s;
typedef struct queue_s Queue_t;

/**
 * alloca e inizializza la coda
 * deve essere chiamata da un solo thread (tipicamente main)
 * 
 * @return NULL se si sono verificati errori nella fase di allocazione o inizializzazione
 * @return puntatore alla coda allocata
 */
Queue_t* initQueue();

/**
 * dealloca e deinizializza una coda precedentemente allocata con initQueue
 * deve essere chiamata da un solo thread (tipicamente il main)
 *
 * @param puntatore alla coda da cancellare
 * @param funzione di deallocazione del dato presente nella coda
*/
void deleteQueue(Queue_t* q, void (*fun)(void*));

/**
 * inserisce un elemento nella coda
 * 
 * @param puntatore alla coda
 * @param dato da inserire
 * 
 * @return 0 sul successo
 * @return -1 se fallimento (dato da inserire NULL)
*/
int lpush(Queue_t* q, void* data);

/**
 * estrae un elemento dalla coda
 * 
 * @param puntatore alla coda
 * @return puntatore al dato da restituire
*/
void* lpop(Queue_t* q);

/**
 * restituisce la dimensione attuale della coda
 * 
 * @param q coda
 * @return numero di elementi nella coda
*/
int lget_size(Queue_t* q);

#endif /* QUEUE_H */