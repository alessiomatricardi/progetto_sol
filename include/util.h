/* utility */
#ifndef UTIL_H
#define UTIL_H
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#define ERR(...)                  \
    fprintf(stderr, __VA_ARGS__); \
    exit(EXIT_FAILURE);

#define CHECK_NULL(ptr) (ptr) == NULL

/** 
 * ferma l'esecuzione per msec millisecondi
 * 
 * @param msec millisecondi
 * @return 0 se la chiamata termina con successo
 * @return -1 se il valore immesso è < 0
*/
int msleep(long msec) {
    struct timespec ts;
    int res;

    if (msec < 0) {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

/* funzioni che settano errno in caso di fallimento di operazioni su mutex e/o var. di condizione */

int mutex_lock(pthread_mutex_t * mutex) {
    int err = 0;
    if((err = pthread_mutex_lock(mutex)) != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

int mutex_unlock(pthread_mutex_t * mutex) {
    int err = 0;
    if ((err = pthread_mutex_unlock(mutex)) != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

int cond_wait(pthread_cond_t * cond, pthread_mutex_t * mutex) {
    int err = 0;
    if ((err = pthread_cond_wait(cond, mutex)) != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

int cond_signal(pthread_cond_t* cond) {
    int err = 0;
    if ((err = pthread_cond_signal(cond)) != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

#endif /* UTIL_H */