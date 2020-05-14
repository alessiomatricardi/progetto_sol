#ifndef CLIENTE_H
#define CLIENTE_H
#include <bqueue.h>

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
    pthread_cond_t* cond_cassa;
    pthread_mutex_t* mutex;
    pthread_cond_t* cond_auth;
    BQueue_t* coda_casse;          /* insieme delle code delle casse */
    /* variabili condivise in sola lettura */
    int thid;         /* thread del cliente */
    int num_prodotti; /* numero dei prodotti acquistati dal cliente, tra 0 e P>0 */
    /* variabili non condivise */
    int tempo_acquisti; /* tempo impiegato per gli acquisti, tra 10 e T>10 millisecondi */
} cliente_opt_t;

/*
 * funzione eseguita da un thread cliente
*/
void* cliente(void* arg);

#endif /* CLIENTE_H */