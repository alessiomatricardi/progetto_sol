#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <bqueue.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <util.h>

struct bqueue_s {
    void** buffer;                     /* array di puntatori a void */
    size_t max_size;                   /* numero massimo di elementi nel buffer, da definire nell'inizializzazione */
    size_t head;                       /* testa della coda, per le estrazioni */
    size_t tail;                       /* fine della coda, per gli inserimenti */
    size_t count;                      /* numero di elementi attualmente presenti nel buffer */
    pthread_mutex_t mutex;             /* variabile di mutua esclusione sulla coda */
    pthread_cond_t cond_empty;         /* variabile di condizione per la coda vuota */
    pthread_condattr_t condempty_attr; /* attributo della variabile di condizione cond_empty */
    pthread_cond_t cond_full;          /* variabile di condizione per la coda piena */
};

BQueue_t* init_BQueue(size_t num) {
    BQueue_t* q = (BQueue_t*)malloc(sizeof(BQueue_t));
    if (CHECK_NULL(q)) {
        errno = EINVAL;
        return NULL;
    }
    q->buffer = (void**)calloc(num, sizeof(void*));
    if (CHECK_NULL(q->buffer)) {
        free(q);
        errno = EINVAL;
        return NULL;
    }
    q->max_size = num;
    q->head = 0;
    q->tail = 0;
    q->count = 0;

    int error = 0;
    if ((error = pthread_mutex_init(&(q->mutex), NULL)) != 0) {
        goto cleanup;
    }
    if ((error = pthread_condattr_init(&q->condempty_attr)) != 0) {
        goto cleanup;
    }
    if ((error = pthread_condattr_setclock(&q->condempty_attr, CLOCK_MONOTONIC)) != 0) {
        goto cleanup;
    }
    if ((error = pthread_cond_init(&(q->cond_empty), &q->condempty_attr)) != 0) {
        goto cleanup;
    }
    if ((error = pthread_cond_init(&(q->cond_full), NULL)) != 0) {
        goto cleanup;
    }
    return q;
cleanup:
    if (&q->mutex) pthread_mutex_destroy(&q->mutex);
    if (&q->cond_empty) pthread_cond_destroy(&q->cond_empty);
    if (&q->cond_full) pthread_cond_destroy(&q->cond_full);
    if (&q->condempty_attr) pthread_condattr_destroy(&q->condempty_attr);
    free(q->buffer);
    free(q);
    errno = error;
    return NULL;
}

void delete_BQueue(BQueue_t* q, void (*fun)(void*)) {
    if (CHECK_NULL(q)) {
        errno = EINVAL;
        return;
    }
    void* data = NULL;
    while (q->head++ < q->tail) {
        data = q->buffer[q->head];
        if (fun) fun(data);
    }
    if (&q->mutex) pthread_mutex_destroy(&q->mutex);
    if (&q->cond_empty) pthread_cond_destroy(&q->cond_empty);
    if (&q->cond_full) pthread_cond_destroy(&q->cond_full);
    if (&q->condempty_attr) pthread_condattr_destroy(&q->condempty_attr);
    free(q->buffer);
    free(q);
}

int push(BQueue_t* q, void* data) {
    if (!q || !data) {
        errno = EINVAL;
        return -1;
    }
    if (mutex_lock(&(q->mutex)) == -1) {
        return -1;
    }
    while (q->count == q->max_size) {
        if (cond_wait(&(q->cond_full), &(q->mutex)) == -1) {
            return -1;
        }
    }
    q->buffer[q->tail] = data;
    q->tail = (q->tail + 1) % q->max_size;
    q->count += 1;
    //printf("prodotto %d\n",*(int*)data);
    if (cond_signal(&(q->cond_empty)) == -1) {
        return -1;
    }
    if (mutex_unlock(&(q->mutex)) == -1) {
        return -1;
    }
    return 0;
}

void* pop(BQueue_t* q, struct timespec* ts) {
    if (!q) {
        errno = EINVAL;
        return NULL;
    }
    if (mutex_lock(&(q->mutex)) == -1) {
        return NULL;
    }
    while (q->count == 0) {
        if (cond_timedwait(&(q->cond_empty), &(q->mutex), ts) == -1) {
            if (errno == ETIMEDOUT) {
                if (mutex_unlock(&(q->mutex)) == -1) {
                    return NULL;
                }
                return NOMORECLIENTS;
            }
            return NULL;
        }
    }
    void* value = q->buffer[q->head];
    q->head = (q->head + 1) % q->max_size;
    q->count -= 1;
    if (cond_signal(&(q->cond_full)) == -1) {
        return NULL;
    }
    if (mutex_unlock(&(q->mutex)) == -1) {
        return NULL;
    }
    return value;
}

int get_size(BQueue_t* q) {
    int length = 0;
    if (mutex_lock(&(q->mutex)) == -1) {
        return -1;
    }
    length = q->count;
    if (mutex_unlock(&(q->mutex)) == -1) {
        return -1;
    }
    return length;
}
