#ifndef CLIENTE_H
#define CLIENTE_H
#include <bool.h>
#include <bqueue.h>
#include <cassa.h>

#define MIN_T_ACQUISTI 10

/* stati di un cliente */
typedef enum _cliente_state {
    ENTRATO,               /* entrato nel supermercato */
    IN_CODA,               /* il cliente si è messo in coda */
    FINITO_CASSA,          /* finito alla cassa, deve uscire */
    CAMBIA_CASSA,          /* la cassa ha chiuso, la deve cambiare */
    USCITA_SENZA_ACQUISTI, /* il cliente esce senza aver acquistato prodotti */
    USCITA_SEGNALE,        /* c'è stata un'emergenza, esci */
} cliente_state_t;

/* struct di supporto di ogni thread cliente */
typedef struct _cliente {
    /* variabili mutabili */

    pthread_mutex_t* mutex_cliente; /* mutex personale di ogni cliente */
    cliente_state_t stato_cliente;  /* stato attuale del cliente */
    pthread_cond_t* cond_incoda;    /* var. condizione personale di ogni cliente */
    pthread_mutex_t* main_mutex;    /* mutex principale */
    bool* is_authorized;            /* autorizzato ad uscire dal direttore se num_prodotti = 0 */
    pthread_cond_t* auth_cond;      /* var. condizione per attesa autorizzazione dal direttore */
    cassa_state_t* stato_casse;     /* array dello stato delle casse */
    BQueue_t** coda_casse;          /* insieme delle code delle casse */
    int* num_casse_attive;          /* casse attualmente aperte */
    pthread_mutex_t* exit_mutex;    /* mutex per segnalare uscita al supermercato */
    pthread_cond_t* exit_cond;      /* var. condizione per dire al supermercato che sono uscito */
    bool* is_exited;                /* uscito dal supermercato */
    int* num_exited;                /* numero di clienti usciti dal supermercato */
    /* variabili immutabili */

    int id_cliente;     /* id del cliente */
    int num_prodotti;   /* numero dei prodotti acquistati dal cliente, tra 0 e P>0 */
    int tempo_acquisti; /* tempo impiegato per gli acquisti, tra 10 e T>10 millisecondi */
    int num_casse_tot;  /* numero di casse totali presenti nel sistema */
    unsigned seed;      /* seed per randomizzazione cassa scelta */
    /* variabili utili per log */

    double t_permanenza;                /* tempo totale di permanenza nel supermercato */
    struct timespec tstart_attesa_coda; /* quando il cliente si mette in coda */
    struct timespec tend_attesa_coda;   /* quando il cliente viene servito */
    int num_cambi_coda;                 /* numero di volte che ha dovuto cambiare la coda */
} cliente_opt_t;

/*
 * funzione eseguita da un thread cliente
*/
void* fun_cliente(void* arg);

#endif /* CLIENTE_H */