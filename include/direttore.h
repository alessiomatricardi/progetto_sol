#ifndef DIRETTORE_H
#define DIRETTORE_H
#include <bool.h>
#include <cassa.h>
#include <signal.h>

/* numero minimo di casse aperte */
#define MIN_CASSE_APERTE 1

typedef enum _direttore_state {
    ATTIVO,             /* il supermercato è regolarmente aperto */
    CHIUSURA,           /* è stato ricevuto il segnale SIGHUP */
    CHIUSURA_IMMEDIATA, /* è stato ricevuto il segnale SIGQUIT */
} direttore_state_t;

typedef direttore_state_t supermercato_state_t;

typedef struct _direttore {
    /* variabili mutabili */
    pthread_mutex_t* quit_mutex;        /* mutex per modificare stato del direttore */
    direttore_state_t* stato_direttore; /* stato attuale del direttore */
    pthread_mutex_t* main_mutex;        /* mutex principale */
    cassa_opt_t* casse;                 /* array di casse */
    pthread_cond_t* auth_cond;          /* var. condizione per autorizzare i clienti */
    bool* auth_array;                   /* array delle autorizzazioni */
    int* queue_notify;                  /* array dove ogni cassa scrive il numero dei clienti in coda */
    int* num_casse_attive;              /* numero attuale delle casse attive */
    /* variabili immutabili */
    int num_clienti;                      /* numero massimo di clienti */
    int casse_tot;                        /* numero totale di clienti */
    int soglia_1;                         /* soglia1 */
    int soglia_2;                         /* soglia2 */
    volatile sig_atomic_t* casse_partite; /* se le casse non sono tutte partite, il direttore non controlla le notifiche */
    unsigned seed;                        /* seed per randomizzazione */
} direttore_opt_t;

void* direttore(void* arg);

#endif /* DIRETTORE_H */