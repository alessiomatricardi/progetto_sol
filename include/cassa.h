#ifndef CASSA_H
#define CASSA_H
#include <bqueue.h>

#define END_OF_SERVICE (void*)0x01
#define MIN_TF_CASSA 20
#define MAX_TF_CASSA 80

typedef enum _cassa_state {
    APERTA,                          /* cassa regolarmente aperta */
    IN_CHIUSURA,                     /* la cassa deve chiudere e fa spostare i clienti */
    CHIUSA,                          /* la cassa è attualmente chiusa */
    CHIUSURA_SUPERMERCATO,           /* la cassa deve servire tutti i clienti in coda e chiudere */
    CHIUSURA_IMMEDIATA_SUPERMERCATO, /* la cassa deve far uscire tutti i clienti */
} cassa_state_t;

typedef struct _cassa {
    /* variabili mutabili */
    pthread_mutex_t* main_mutex; /* mutex principale */
    cassa_state_t* stato_cassa;  /* stato attuale della cassa */
    pthread_cond_t* cond;        /* var. condizione per attesa di essere aperta */
    BQueue_t* coda;              /* coda della cassa */
    int* queue_size_notify;      /* puntatore dove salvare la grandezza attuale della coda */
    /* variabili immutabili */
    int id_cassa;            /* id della cassa */
    int tempo_fisso;         /* tempo fisso impiegato per ogni cliente */
    int tempo_prodotto;      /* tempo variabile speso per ogni prodotto */
    int intervallo_notifica; /* tempo che intercorre tra una notifica e l'altra */
} cassa_opt_t;

void* cassa(void* arg);

#endif /* CASSA_H */