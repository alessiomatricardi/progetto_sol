#include <errno.h>
#include <queue.h>

/*
 * struttura dati nodo di una lista
*/
struct node_s {
    void* data;
    struct node_s* next;
};
typedef struct node_s node_t;

struct queue_s {
    node_t* head;              /* puntatore alla testa, dove rimuovo */
    node_t* tail;              /* puntatore alla coda, dove inserisco */
    unsigned long count;       /* numero di elementi attualmente presenti nella coda */
};

Queue_t* initQueue() {
    Queue_t* q = (Queue_t*)malloc(sizeof(Queue_t));
    if (q == NULL) {
        errno = EINVAL;
        return NULL;
    }
    q->head = NULL;
    q->tail = NULL;
    q->count = 0;
    return q;
}

void deleteQueue(Queue_t* q, void (*fun)(void*)) {
    if (q == NULL) {
        errno = EINVAL;
        return;
    }
    void* data = NULL;
    node_t* p = q->head;
    node_t* p2;
    while (p != NULL) {
        data = p->data;
        if (fun) fun(data);
        p2 = p;
        p = p->next;
        free(p2);
    }
    free(q);
}

int lpush(Queue_t* q, void* data) {
    if (!q || !data) {
        errno = EINVAL;
        return -1;
    }
    node_t* temp = malloc(sizeof(node_t));
    if (temp == NULL) {
        return -1;
    }
    temp->data = data;
    temp->next = NULL;
    if (q->count == 0) { /* coda vuota */
        q->head = temp;
        q->tail = temp;
    } else {
        q->tail->next = temp;
        q->tail = temp;
    }
    (q->count)++;
    //printf("prodotto %d\n",*(int*)data);
    return 0;
}

void* lpop(Queue_t* q) {
    if (!q) {
        errno = EINVAL;
        return NULL;
    }
    if (q->count == 0) {
        return NULL;
    }
    void* value = q->head->data;
    node_t* temp = q->head;
    q->head = q->head->next;
    free(temp);
    (q->count)--;
    return value;
}

int lget_size(Queue_t* q) {
    return q->count;
}