#ifndef DIRETTORE_H
#define DIRETTORE_H
#include <cassa.h>
#include <stdlib.h>

/* numero minimo di casse aperte */
#define MIN_CASSE_APERTE 1

typedef enum _direttore_state {
    ATTIVO,
    CHIUSURA,
    CHIUSURA_IMMEDIATA,
} direttore_state_t;

typedef struct _direttore {
    direttore_state_t stato_direttore;
    pthread_mutex_t* mutex_direttore;
    cassa_opt_t* casse;
    pthread_mutex_t* mutex;
    pthread_cond_t* cond_auth;
    size_t k_max;
    size_t soglia_1;
    size_t soglia_2;
} direttore_opt_t;

void* direttore(void* arg);

#endif /* DIRETTORE_H */