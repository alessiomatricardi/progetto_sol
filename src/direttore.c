#define _POSIX_C_SOURCE 200112L
#include <direttore.h>
#include <util.h>
#include <string.h>
#include <signal.h>

/* pid del processo */
extern pid_t pid;

static void autorizza_uscita(bool* auth_array, int num_clienti, pthread_mutex_t* mtx, pthread_cond_t* cond);

void* direttore(void* arg) {
    /* da vedere */
    int sig, error = 0;
    sigset_t sigmask;
    error = sigemptyset(&sigmask);
    error |= sigaddset(&sigmask, SIGHUP);
    error |= sigaddset(&sigmask, SIGQUIT);
    error |= sigaddset(&sigmask, SIGUSR1);
    pthread_sigmask(SIG_SETMASK, &sigmask, NULL);
    /* da vedere */

    direttore_opt_t* direttore = (direttore_opt_t*)arg;
    direttore_state_t * stato = direttore->stato_direttore;
    while(1) {
        // controlla stato
        if (mutex_lock(direttore->quit_mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
        if(*stato != ATTIVO) {
            if (mutex_unlock(direttore->quit_mutex) != 0) {
                perror("cliente mutex lock 2");
                // gestire errore
            }
            break;
        }
        if (mutex_unlock(direttore->quit_mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
        // controlla notifiche
        if (mutex_lock(direttore->main_mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
        for (size_t i = 0; i < direttore->casse_tot; i++) {
            //printf("%d ",direttore->queue_notify[i]);
        }
        //printf("\n");
        fflush(stdout);
        if (mutex_unlock(direttore->main_mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
        // risveglia clienti che attendono autorizzazione per uscire
        autorizza_uscita(direttore->auth_array, direttore->num_clienti, direttore->main_mutex, direttore->auth_cond);
    }
    printf("arrivato qui\n");
    if(*stato == CHIUSURA) {
        /* tutte le casse vanno in stato di CHIUSURA */
        if (mutex_lock(direttore->main_mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
        for(size_t i = 0; i < direttore->casse_tot; i++) {
            *(direttore->casse[i].stato_cassa) = CHIUSURA_SUP_CASSA;
            while(push(direttore->casse[i].coda, END_OF_SERVICE) != 0) {
                perror("push");
                // gestione errore
            }
            if(cond_signal(direttore->casse[i].cond) != 0) {
                perror("signal cassa");
            }
        }
        if (mutex_unlock(direttore->main_mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
    } else { /* *stato == CHIUSURA_IMMEDIATA */

    }
    pthread_exit((void*)0);
}

static void autorizza_uscita(bool* auth_array, int num_clienti, pthread_mutex_t* mtx, pthread_cond_t* cond) {
    if (mutex_lock(mtx) != 0) {
        perror("cliente mutex lock 2");
        // gestire errore
    }
    for(size_t i = 0; i < num_clienti; i++) {
        auth_array[i] = true;
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