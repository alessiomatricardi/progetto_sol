#define _POSIX_C_SOURCE 200112L
#include <cassa.h>
#include <cliente.h>
#include <logger.h>
#include <signal.h>
#include <util.h>
#include <time.h>
#include <bool.h>

/* pid del processo */
extern pid_t pid;

static bool check_continue(cassa_opt_t* cassa, cassa_state_t* stato);
static void set_stato_cliente(cliente_opt_t* cliente, cliente_state_t stato_cliente);
static void libera_cassa(cassa_opt_t* cassa, cassa_state_t stato);
static void invia_notifica(cassa_opt_t* cassa);

void* cassa(void* arg) {
    /* maschera segnali */
    int error = 0;
    sigset_t sigmask;
    error = sigemptyset(&sigmask);
    error |= sigaddset(&sigmask, SIGHUP);
    error |= sigaddset(&sigmask, SIGQUIT);
    error |= sigaddset(&sigmask, SIGUSR1);
    if ((error |= pthread_sigmask(SIG_SETMASK, &sigmask, NULL)) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }

    cassa_opt_t* cassa = (cassa_opt_t*)arg;
    int t_notifica = cassa->intervallo_notifica;
    cassa_state_t* stato = cassa->stato_cassa;

    /* per valutare il tempo che intercorre tra una notifica ed un'altra */
    struct timespec tstart = {0, 0}, tend = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tstart);

    bool res = true;
    
    while (1) {
        /* controllo di essere aperta */
        res = check_continue(cassa, stato);
        if(!res) break;

        /* devo servire un cliente */
        void* temp_cliente = pop(cassa->coda);
        if(CHECK_NULL(temp_cliente)) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }

        /* la cassa era aperta ma aspettava cliente in coda
         * -> non ha clienti da servire o da invitare ad uscire
         * -> esce senza far nulla
        */
        if (temp_cliente == END_OF_SERVICE) {
            pthread_exit((void*)0);
        }

        cliente_opt_t* cliente = (cliente_opt_t*)temp_cliente;

        int t_servizio = cassa->tempo_fisso + cassa->tempo_prodotto * cliente->num_prodotti;
        while (t_servizio >= t_notifica) {
            msleep(t_notifica);
            
            // invia notifica
            invia_notifica(cassa);

            /* per valutare il tempo che intercorre tra una notifica ed un'altra */
            clock_gettime(CLOCK_MONOTONIC, &tend);
            LOG_DEBUG("notify sent after %.5f seconds",
                      ((double)tend.tv_sec + 1.0e-9 * tend.tv_nsec) -
                          ((double)tstart.tv_sec + 1.0e-9 * tstart.tv_nsec));
            tstart = tend;

            t_servizio -= t_notifica;
            t_notifica = cassa->intervallo_notifica;
        }
        msleep(t_servizio);
        t_notifica = t_notifica - t_servizio;

        set_stato_cliente(cliente, FINITO_CASSA);
    }
    LOG_DEBUG("cassa %d, chiusura ricevuta", cassa->id_cassa);

    libera_cassa(cassa, *stato);
    pthread_exit((void*)0);
}

static bool check_continue(cassa_opt_t* cassa, cassa_state_t* stato){
    if (mutex_lock(cassa->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    if (*stato == IN_CHIUSURA) {
        libera_cassa(cassa, IN_CHIUSURA);
        *stato = CHIUSA;
    }
    while (*stato == CHIUSA) {
        if (cond_wait(cassa->cond, cassa->main_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
    }
    if (*stato != APERTA) { /* ricevuto segnale */
        if (mutex_unlock(cassa->main_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        return false;
    }
    if (mutex_unlock(cassa->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    return true;
}

static void set_stato_cliente(cliente_opt_t* cliente, cliente_state_t stato_cliente) {
    if (mutex_lock(cliente->mutex_cliente) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    cliente->stato_cliente = stato_cliente;
    if (cond_signal(cliente->cond_incoda) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    if (mutex_unlock(cliente->mutex_cliente) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}

static void libera_cassa(cassa_opt_t* cassa, cassa_state_t stato) {
    void* tmp_cliente;
    while (get_size(cassa->coda) > 0) {
        tmp_cliente = pop(cassa->coda);
        if(CHECK_NULL(tmp_cliente)) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        if (tmp_cliente == END_OF_SERVICE) break;
        cliente_opt_t* cliente = (cliente_opt_t*)tmp_cliente;
        if (stato == IN_CHIUSURA) {
            set_stato_cliente(cliente, CAMBIA_CASSA);
        } else if (stato == CHIUSURA_SUPERMERCATO) {
            int t_servizio = cassa->tempo_fisso + cassa->tempo_prodotto * cliente->num_prodotti;
            msleep(t_servizio);
            set_stato_cliente(cliente, USCITA_SEGNALE);
        } else { /* stato == CHIUSURA_IMMEDIATA_SUPERMERCATO */
            set_stato_cliente(cliente, USCITA_SEGNALE);
        }
    }
}

static void invia_notifica(cassa_opt_t* cassa) {
    int size = get_size(cassa->coda);
    if (size == -1) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    if (mutex_lock(cassa->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    *(cassa->queue_size_notify) = size;
    if (mutex_unlock(cassa->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}