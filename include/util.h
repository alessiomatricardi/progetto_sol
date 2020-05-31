/* utility */
#ifndef UTIL_H
#define UTIL_H
#include <pthread.h>

#define CHECK_NULL(ptr) (ptr) == NULL

/**
 * ritorna la differenza in secondi tra due tempi
 * 
 * @param start tempo iniziale
 * @param end tempo finale
 * @return differenza in secondi tra i due tempi
*/
double spec_difftime(const struct timespec start, const struct timespec end);

/** 
 * ferma l'esecuzione per msec millisecondi
 * 
 * @param msec millisecondi
 * @return 0 se la chiamata termina con successo
 * @return -1 se il valore immesso è < 0
*/
int msleep(long msec);

/**
 * lock a mutex
 * 
 * @param mutex mutex to lock
 * @return 0 if success
 * @return -1 if failure, sets errno
*/
int mutex_lock(pthread_mutex_t* mutex);

/**
 * unlock a mutex
 * 
 * @param mutex mutex to unlock
 * @return 0 if success
 * @return -1 if failure, sets errno
*/
int mutex_unlock(pthread_mutex_t* mutex);

/**
 * rilascia mutex e mette il thread in pausa
 * fino a che non viene chiamata una pthread_cond_signal su cond 
 * 
 * @param cond condition variable
 * @param mutex mutex
 * @return 0 if success
 * @return -1 if failure, sets errno
*/
int cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);

/**
 * rilascia mutex e mette il thread in pausa
 * fino a che non viene chiamata una pthread_cond_signal su cond
 * oppure arriva il momento di svegliarsi
 * 
 * @param cond condition variable
 * @param mutex mutex
 * @param ts absolute time
 * @return 0 if success
 * @return -1 if failure, sets errno
*/
int cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex, struct timespec* ts);

/**
 * riattiva un thread che aveva fatto una wait su cond
 * 
 * @param cond condition variable
 * @return 0 if success
 * @return -1 if failure, sets errno
*/
int cond_signal(pthread_cond_t* cond);

/**
 * riattiva tutti i thread che avevano fatto una wait su cond
 * 
 * @param cond condition variable
 * @return 0 if success
 * @return -1 if failure, sets errno
*/
int cond_broadcast(pthread_cond_t* cond);

#endif /* UTIL_H */