#ifndef CASSA_H
#define CASSA_H
#include <bqueue.h>
#include <stdlib.h>

#define END_OF_SERVICE (void*)-1

typedef enum _cassa_state {
    APERTA,
    CHIUSA,
    TERMINAZIONE,
    TERMINAZIONE_IMMEDIATA,
} cassa_state_t;

typedef struct _cassa {
    cassa_state_t stato_cassa;
    BQueue_t coda;
    pthread_mutex_t* mutex;
    pthread_cond_t* cond;
    int thid;
    int tempo_fisso;
    int tempo_prodotto;
    int intervallo_notifica;
} cassa_opt_t;

void* cassa(void* arg);

#endif /* CASSA_H */