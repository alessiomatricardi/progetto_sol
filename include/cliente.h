#ifndef CLIENTE_H
#define CLIENTE_H
#include <bool.h>
#include <bqueue.h>
#include <cassa.h>

#define MIN_T_ACQUISTI 10

/* stati di un cliente */
typedef enum _cliente_state {
    ENTRATO,
    FINITO_CASSA,
    CAMBIA_CASSA,
    USCITA_SEGNALE,
} cliente_state_t;

/* struct di supporto di ogni thread cliente */
typedef struct _cliente {
    /* variabili condivise in scrittura con altri thread */
    cliente_state_t stato_cliente; /* stato attuale del cliente */
    pthread_mutex_t* mutex_cliente;
    pthread_cond_t* cond_incoda;
    pthread_mutex_t* main_mutex;
    bool* authorized;
    pthread_cond_t* auth_cond;
    cassa_state_t* stato_casse;
    BQueue_t** coda_casse; /* insieme delle code delle casse */
    int* casse_attive;
    pthread_mutex_t* exit_mutex;
    bool* is_exited;
    int* num_exited;
    int casse_tot;
    /* variabili condivise in sola lettura */
    int num_prodotti; /* numero dei prodotti acquistati dal cliente, tra 0 e P>0 */
    /* variabili non condivise */
    int tempo_acquisti; /* tempo impiegato per gli acquisti, tra 10 e T>10 millisecondi */
    unsigned seed;
} cliente_opt_t;

/*
 * funzione eseguita da un thread cliente
*/
void* cliente(void* arg);

#endif /* CLIENTE_H */