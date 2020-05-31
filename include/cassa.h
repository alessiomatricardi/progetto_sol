#ifndef CASSA_H
#define CASSA_H
#include <bool.h>
#include <bqueue.h>
#include <queue.h>

#define MIN_TF_CASSA 20
#define MAX_TF_CASSA 80

typedef enum _cassa_state {
    APERTA,                /* cassa regolarmente aperta */
    CHIUSA,                /* la cassa è attualmente chiusa */
    SERVI_E_TERMINA,       /* la cassa era aperta e deve servire tutti i clienti in coda e chiudere */
    NON_SERVIRE_E_TERMINA, /* la cassa era aperta e deve far uscire tutti i clienti */
} cassa_state_t;

typedef struct _cassa {
    /* variabili mutabili */

    pthread_mutex_t* main_mutex; /* mutex principale */
    cassa_state_t* stato_cassa;  /* stato attuale della cassa */
    BQueue_t* coda;              /* coda della cassa */
    pthread_cond_t* notify_cond; /* var. condizione su cui il direttore aspetta che tutti abbiano notificato */
    int* notify_size;            /* puntatore dove salvare la grandezza attuale della coda */
    bool* notify_sent;           /* puntatore dove i cassieri dicono di aver notificato */
    /* variabili immutabili */

    int id_cassa;            /* id della cassa */
    int tempo_fisso;         /* tempo fisso impiegato per ogni cliente */
    int tempo_prodotto;      /* tempo variabile speso per ogni prodotto */
    int intervallo_notifica; /* tempo che intercorre tra una notifica e l'altra */
    /* variabili utili per log */

    int num_clienti_serviti;     /* numero di clienti serviti */
    long num_prodotti_elaborati; /* numero totale di prodotti elaborati */
    Queue_t* tempi_apertura;     /* lista dei tempi di apertura */
    int num_chiusure;            /* numero di chiusure */
    Queue_t* t_clienti_serviti;  /* tempo di servizio di ogni cliente servito */
} cassa_opt_t;

void* fun_cassa(void* arg);

#endif /* CASSA_H */