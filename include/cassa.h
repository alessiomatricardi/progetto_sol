#ifndef CASSA_H
#define CASSA_H
#include <bqueue.h>

#define END_OF_SERVICE (void*)0x01
#define MIN_TF_CASSA 20
#define MAX_TF_CASSA 80

typedef enum _cassa_state {
    APERTA,
    CHIUSA,
    CHIUSURA_SUP_CASSA,
    CHIUSURA_IMM_SUP_CASSA,
} cassa_state_t;

typedef struct _cassa {
    cassa_state_t* stato_cassa;
    BQueue_t* coda;
    pthread_mutex_t* main_mutex;
    pthread_cond_t* cond;
    int* queue_size;
    int tempo_fisso;
    int tempo_prodotto;
    int intervallo_notifica;
} cassa_opt_t;

void* cassa(void* arg);

#endif /* CASSA_H */