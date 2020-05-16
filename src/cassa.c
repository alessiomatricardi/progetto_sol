#define _POSIX_C_SOURCE 200112L
#include <cassa.h>
#include <cliente.h>
#include <util.h>
#include <signal.h>

/* pid del processo */
extern pid_t pid;

static void invia_notifica(cassa_opt_t* cassa);

void* cassa(void* arg) {
    /* da vedere */
    int sig, error = 0;
    sigset_t sigmask;
    error = sigemptyset(&sigmask);
    error |= sigaddset(&sigmask, SIGHUP);
    error |= sigaddset(&sigmask, SIGQUIT);
    error |= sigaddset(&sigmask, SIGUSR1);
    pthread_sigmask(SIG_SETMASK, &sigmask, NULL);
    /* da vedere */

    cassa_opt_t* cassa = (cassa_opt_t*)arg;
    int t_notifica = cassa->intervallo_notifica;
    cassa_state_t stato;
    while (1) {
        if (mutex_lock(cassa->main_mutex) != 0) {
            perror("mutex cassa");
        }
        while ((stato = *(cassa->stato_cassa)) == CHIUSA) {
            if (cond_wait(cassa->cond, cassa->main_mutex) != 0) {
                perror("cond");
            }
        }
        if (stato != APERTA) {
            if (mutex_unlock(cassa->main_mutex) != 0) {
                perror("mutex cassa");
            }
            break;
        }
        if (mutex_unlock(cassa->main_mutex) != 0) {
            perror("mutex cassa");
        }
        void* temp_cliente = pop(cassa->coda);
        /* la cassa era aperta ma aspettava cliente in coda
         * -> non ha clienti da servire o da invitare ad uscire
         * -> esce senza far nulla
        */
        if (temp_cliente == END_OF_SERVICE) {
            pthread_exit((void*)0);
        }
        cliente_opt_t* cliente = (cliente_opt_t*)temp_cliente;

        int t_servizio = cassa->tempo_fisso + cassa->tempo_prodotto * cliente->num_prodotti;
        if (t_servizio >= t_notifica) {
            while (t_servizio >= t_notifica) {
                msleep(t_notifica);
                // invia notifica
                invia_notifica(cassa);
                t_servizio -= t_notifica;
                t_notifica = cassa->intervallo_notifica;
            }
            msleep(t_servizio);
            t_notifica = (t_notifica - t_servizio) % cassa->intervallo_notifica;
        } else {
            msleep(t_servizio);
            t_notifica = t_notifica - t_servizio;
        }

        if (mutex_lock(cliente->mutex_cliente) != 0) {
            perror("mutex cassa");
        }
        cliente->stato_cliente = FINITO_CASSA;
        if(cond_signal(cliente->cond_incoda) != 0) {
            perror("signal client");
        }
        if (mutex_unlock(cliente->mutex_cliente) != 0) {
            perror("mutex cassa");
        }
    }
    if (stato == CHIUSURA_SUP_CASSA) {
        // FLUSH CLIENTI servili tutti
        void* tmp_cliente;
        printf("Ricevuta chiusura\n");
        while(get_size(cassa->coda) > 0) {
            tmp_cliente = pop(cassa->coda);
            if(tmp_cliente == END_OF_SERVICE) break;
            cliente_opt_t* cliente = (cliente_opt_t*)tmp_cliente;
            if (mutex_lock(cliente->mutex_cliente) != 0) {
                perror("mutex cassa");
            }
            cliente->stato_cliente = USCITA_SEGNALE;
            if (cond_signal(cliente->cond_incoda) != 0) {
                perror("signal client");
            }
            if (mutex_unlock(cliente->mutex_cliente) != 0) {
                perror("mutex cassa");
            }
        }
    } else { /* stato = CHIUSURA_IMMEDIATA */
    }
    pthread_exit((void*)0);
}

static void invia_notifica(cassa_opt_t* cassa) {
    int size = get_size(cassa->coda);
    if(size == -1) {
    }
    if (mutex_lock(cassa->main_mutex) != 0) {
        perror("mutex cassa");
    }
    *(cassa->queue_size) = size;
    if (mutex_unlock(cassa->main_mutex) != 0) {
        perror("mutex cassa");
    }
}