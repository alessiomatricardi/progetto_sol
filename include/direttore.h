#ifndef DIRETTORE_H
#define DIRETTORE_H
#include <cassa.h>
#include <bool.h>

/* numero minimo di casse aperte */
#define MIN_CASSE_APERTE 1

typedef enum _direttore_state {
    ATTIVO,
    CHIUSURA,
    CHIUSURA_IMMEDIATA,
} direttore_state_t;

typedef direttore_state_t supermercato_state_t;

typedef struct _direttore {
    direttore_state_t stato_direttore;
    pthread_mutex_t* quit_mutex;
    cassa_opt_t* casse;
    pthread_mutex_t* main_mutex;
    pthread_cond_t* auth_cond;
    bool* auth_array;
    int* queue_notify;
    int num_clienti;
    int casse_tot;
    int* casse_attive;
    int soglia_1;
    int soglia_2;
} direttore_opt_t;

void* direttore(void* arg);

#endif /* DIRETTORE_H */