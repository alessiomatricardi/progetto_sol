#define _POSIX_C_SOURCE 200112L
#include <direttore.h>
#include <util.h>

void autorizza_uscita(pthread_mutex_t* mtx, pthread_cond_t* cond);

    void* direttore(void* arg) {
    direttore_opt_t* direttore = (direttore_opt_t*)arg;
    direttore_state_t stato;
    while(1) {
        // controlla stato
        if (mutex_lock(direttore->mutex_direttore) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
        if((stato = direttore->stato_direttore) != ATTIVO) { break; }
        if (mutex_unlock(direttore->mutex_direttore) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
        // controlla notifiche - chiudi casse
        // risveglia clienti che attendono autorizzazione per uscire
        autorizza_uscita(direttore->mutex, direttore->cond_auth);
    }
    if (mutex_unlock(direttore->mutex_direttore) != 0) {
        perror("cliente mutex lock 2");
        // gestire errore
    }
    printf("arrivato qui\n");
    if(stato == CHIUSURA) {
        /* tutte le casse vanno in stato di CHIUSURA */
        if (mutex_lock(direttore->mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
        for(size_t i = 0; i < direttore->k_max; i++) {
            direttore->casse[i].stato_cassa = CHIUSURA_SUP_CASSA;
            while(push(direttore->casse[i].coda, END_OF_SERVICE) != 0) {
                perror("push");
                // gestione errore
            }
            if(cond_signal(direttore->casse[i].cond) != 0) {
                perror("signal cassa");
            }
        }
        if (mutex_unlock(direttore->mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
    } else { /* stato == CHIUSURA_IMMEDIATA */

    }
    pthread_exit((void*)0);
}

void autorizza_uscita(pthread_mutex_t* mtx, pthread_cond_t* cond) {
    if (mutex_lock(mtx) != 0) {
        perror("cliente mutex lock 2");
        // gestire errore
    }
    if (cond_broadcast(cond) != 0) {
        perror("direttore cond broadcast 2");
        // GESTIRE ERRORE
    }
    if (mutex_unlock(mtx) != 0) {
        perror("cliente mutex lock 2");
        // gestire errore
    }
}