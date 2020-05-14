#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <bqueue.h>
#include <string.h>

BQueue_t* init_BQueue(size_t num) {
    if(num <= 0) {
        errno = EINVAL;
        return NULL;
    }
    BQueue_t* q = (BQueue_t*)malloc(sizeof(BQueue_t));
    if (CHECK_NULL(q)) {
        errno = EINVAL;
        return NULL;
    }
    q->buffer = (void**)malloc(num*sizeof(void*));
    if(CHECK_NULL(q->buffer)) {
        return NULL;
    }
    q->max_size = num;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    int myerror = 0;
    if (pthread_mutex_init(&(q->mutex), NULL) != 0) {
        perror("while init mutex");
        myerror = errno;
        free(q);
        errno = myerror;
        return NULL;
    }
    if (pthread_cond_init(&(q->cond_empty), NULL) != 0) {
        perror("while init cond_empty");
        myerror = errno;
        pthread_mutex_destroy(&q->mutex);
        free(q);
        errno = myerror;
        return NULL;
    }
    if (pthread_cond_init(&(q->cond_full), NULL) != 0) {
        perror("while init cond_full");
        myerror = errno;
        pthread_mutex_destroy(&q->mutex);
        pthread_cond_destroy(&q->cond_empty);
        free(q);
        errno = myerror;
        return NULL;
    }
    return q;
}

void delete_BQueue(BQueue_t* q, void (*fun)(void*)) {
    if (CHECK_NULL(q)) {
        errno = EINVAL;
        return;
    }
    void* data = NULL;
    while (q->head++ < q->tail) {
        data = q->buffer[q->head];
        if(fun) fun(data);
    }
    if (&q->mutex) pthread_mutex_destroy(&q->mutex);
    if (&q->cond_empty) pthread_cond_destroy(&q->cond_empty);
    if (&q->cond_full) pthread_cond_destroy(&q->cond_full);
    free(q->buffer);
    free(q);
}

int push(BQueue_t* q, void* data) {
    if (!q || !data) {
        errno = EINVAL;
        return -1;
    }
    if(mutex_lock(&(q->mutex)) == -1) {
        return -1;
    }
    while(q->count == q->max_size) {
        if(cond_wait(&(q->cond_full), &(q->mutex)) == -1) {
            return -1;
        }
    }
    q->buffer[q->tail] = data;
    q->tail = (q->tail + 1) % q->max_size;
    (q->count)++;
    //printf("prodotto %d\n",*(int*)data);
    if(cond_signal(&(q->cond_empty)) == -1) {
        return -1;
    }
    if(mutex_unlock(&(q->mutex)) == -1) {
        return -1;
    }
    return 0;
}

void* pop(BQueue_t* q) {
    if (!q) {
        errno = EINVAL;
        return NULL;
    }
    if (mutex_lock(&(q->mutex)) == -1) {
        return NULL;
    }
    while (q->count == 0) {
        if (cond_wait(&(q->cond_empty), &(q->mutex)) == -1) {
            return NULL;
        }
    }
    void* value = q->buffer[q->head];
    q->head = (q->head + 1) % q->max_size;
    (q->count)--;
    if (cond_signal(&(q->cond_full)) == -1) {
        return NULL;
    }
    if (mutex_unlock(&(q->mutex)) == -1) {
        return NULL;
    }
    return value;
}
