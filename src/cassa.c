#define _POSIX_C_SOURCE 200112L
#include <bool.h>
#include <cassa.h>
#include <cliente.h>
#include <logger.h>
#include <signal.h>
#include <time.h>
#include <util.h>

#define BILLION 1000000000L
#define MILLION 1000000
#define SEC_TO_MSEC 1000

/* pid del processo */
extern pid_t pid;

/* per informare cassieri e direttore quando è il momento di chiudere definitivamente */
extern volatile int should_quit;

/**
 * aggiunge msec millisecondi al tempo attuale
*/
static void add_to_now(struct timespec* ts, int msec) {
    clock_gettime(CLOCK_MONOTONIC, ts);
    if (msec <= 0) return;
    ts->tv_sec += msec / 1000;
    ts->tv_nsec += (msec % 1000) * MILLION;
    if (ts->tv_nsec >= BILLION) {
        ts->tv_sec += (ts->tv_nsec / BILLION);
        ts->tv_nsec -= BILLION;
    }
}
static cassa_state_t get_stato_cassa(cassa_opt_t* cassa);
static void add_tapertura(cassa_opt_t* cassa, struct timespec tstart_apertura);
static bool check_apertura(cassa_opt_t* cassa, struct timespec tstart_apertura);
static void set_stato_cliente(cliente_opt_t* cliente, cliente_state_t stato_cliente);
static void libera_cassa(cassa_opt_t* cassa);
static void chiusura_definitiva(cassa_opt_t* cassa, struct timespec tstart_apertura);
static void invia_notifica(cassa_opt_t* cassa, struct timespec* tstart_notifica);

void* fun_cassa(void* arg) {
    if (CHECK_NULL(arg)) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
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

    bool res = true;
    int t_notifica = cassa->intervallo_notifica;
    void* temp_cliente = NULL;
    struct timespec tstart_apertura = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tstart_apertura);

    /* per valutare il tempo che intercorre tra una notifica ed un'altra */
    struct timespec tstart_notifica = {0, 0};
    struct timespec ts_notifica = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tstart_notifica);

    while (1) {
        /* controllo di essere aperta */
        res = check_apertura(cassa, tstart_apertura);
        if (!res) break;

        /* devo servire un cliente */
        add_to_now(&ts_notifica, t_notifica);
        temp_cliente = pop(cassa->coda, &ts_notifica);
        if (CHECK_NULL(temp_cliente)) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }

        // non c'è alcun cliente da servire
        if (temp_cliente == NOMORECLIENTS) {
            // invia notifica
            invia_notifica(cassa, &tstart_notifica);
            t_notifica = cassa->intervallo_notifica;
            continue;
        }
        // c'è un cliente da servire
        cliente_opt_t* cliente = (cliente_opt_t*)temp_cliente;
        clock_gettime(CLOCK_MONOTONIC, &cliente->tend_attesa_coda);

        /* ricalcolo t_notifica */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double msec_diff = SEC_TO_MSEC * spec_difftime(now, ts_notifica);
        t_notifica = (int)msec_diff + ((msec_diff - (int)msec_diff) > 0 ? 1 : 0);
        
        // calcolo tempo di servizio
        if (mutex_lock(cliente->mutex_cliente) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        int t_servizio = cassa->tempo_fisso + cassa->tempo_prodotto * cliente->num_prodotti;
        cassa->num_prodotti_elaborati += cliente->num_prodotti;
        if (mutex_unlock(cliente->mutex_cliente) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        // aggiungo tempo di servizio alla lista
        int* t_serv;
        if (CHECK_NULL(t_serv = malloc(sizeof(int)))) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        *t_serv = t_servizio;
        if (lpush(cassa->t_clienti_serviti, t_serv) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        /**
         * servo il cliente e, se necessario,
         * invio notifiche al direttore
        */ 
        while (t_servizio >= t_notifica) {
            msleep(t_notifica);

            // invia notifica
            invia_notifica(cassa, &tstart_notifica);

            t_servizio -= t_notifica;
            t_notifica = cassa->intervallo_notifica;
        }
        msleep(t_servizio);
        t_notifica -= t_servizio;

        // setto lo stato del cliente
        set_stato_cliente(cliente, FINITO_CASSA);
        cassa->num_clienti_serviti++;
    }
    
    LOG_DEBUG("cassa %d - chiusura ricevuta", cassa->id_cassa);
    chiusura_definitiva(cassa, tstart_apertura);
    LOG_DEBUG("cassa %d - chiude definitivamente", cassa->id_cassa);
    return NULL;
}

/**
 * restituisce lo stato della cassa
*/
static cassa_state_t get_stato_cassa(cassa_opt_t* cassa) {
    cassa_state_t stato;
    if (mutex_lock(cassa->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    stato = *(cassa->stato_cassa);
    if (mutex_unlock(cassa->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    return stato;
}

/**
 * calcola il tempo di apertura della cassa
 * e lo aggiunge alla lista dei tempi
*/
static void add_tapertura(cassa_opt_t* cassa, struct timespec tstart_apertura) {
    cassa->num_chiusure++;
    struct timespec tend_apertura = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tend_apertura);
    double* t_apertura;
    if (CHECK_NULL(t_apertura = malloc(sizeof(double)))) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    *t_apertura = spec_difftime(tstart_apertura, tend_apertura);
    if (lpush(cassa->tempi_apertura, t_apertura) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}

/**
 * controlla che la cassa sia aperta
 * se è chiusa libera la cassa (i clienti in coda dovranno cambiarla)
 * se non è nè aperta nè chiusa -> il supermercato sta chiudendo
 * quindi dovrà chiudere di conseguenza anch'essa
*/
static bool check_apertura(cassa_opt_t* cassa, struct timespec tstart_apertura) {
    if (mutex_lock(cassa->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    if (*(cassa->stato_cassa) == CHIUSA) {
        libera_cassa(cassa);
        add_tapertura(cassa, tstart_apertura);
        if (mutex_unlock(cassa->main_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        pthread_exit((void*)0);
    }
    if (*(cassa->stato_cassa) != APERTA) { /* ricevuto segnale */
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

/**
 * modifica lo stato del cliente
*/
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

/**
 * chiamata quando il direttore decide di chiudere la cassa
 * itera controllando la coda:
 * se c'è un cliente, a questo viene indicato di cambiare cassa
 * altrimenti ha finito il controllo
*/
static void libera_cassa(cassa_opt_t* cassa) {
    void* tmp_cliente;
    struct timespec ts = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    while (1) {
        tmp_cliente = pop(cassa->coda, &ts);
        if (CHECK_NULL(tmp_cliente)) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        if (tmp_cliente == NOMORECLIENTS) break;
        cliente_opt_t* cliente = (cliente_opt_t*)tmp_cliente;
        set_stato_cliente(cliente, CAMBIA_CASSA);
    }
}

/**
 * chiamata quando arriva un segnale sighup o sigquit al supermercato
 * itera controllando la coda ogni 100 millisecondi
 * (aspetta fino a che ci sono clienti all'interno del supermercato):
 * se c'è un cliente e il segnale era sighup, lo serve
 * se c'è un cliente e il segnale era sigquit, lo invita ad uscire
 * se non c'è un cliente, torna a guardare la coda
*/
static void chiusura_definitiva(cassa_opt_t* cassa, struct timespec tstart_apertura) {
    void* tmp_cliente;
    struct timespec ts = {0, 0};
    cassa_state_t stato = get_stato_cassa(cassa);
    while (!should_quit) {
        add_to_now(&ts, 100);

        tmp_cliente = pop(cassa->coda, &ts);
        if (CHECK_NULL(tmp_cliente)) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        // non c'è alcun cliente
        if (tmp_cliente == NOMORECLIENTS) continue;
        // cliente da servire
        cliente_opt_t* cliente = (cliente_opt_t*)tmp_cliente;
        clock_gettime(CLOCK_MONOTONIC, &cliente->tend_attesa_coda);
        /*
         * nel primo ramo il cliente viene servito,
         * nel ramo else viene invitato ad uscire
        */
        if (stato == SERVI_E_TERMINA) {
            // calcolo tempo di servizio
            if (mutex_lock(cliente->mutex_cliente) != 0) {
                LOG_CRITICAL;
                kill(pid, SIGUSR1);
            }
            int t_servizio = cassa->tempo_fisso + cassa->tempo_prodotto * cliente->num_prodotti;
            cassa->num_prodotti_elaborati += cliente->num_prodotti;
            if (mutex_unlock(cliente->mutex_cliente) != 0) {
                LOG_CRITICAL;
                kill(pid, SIGUSR1);
            }

            // aggiungo tempo di servizio alla lista dei tempi
            int* t_serv;
            if (CHECK_NULL(t_serv = malloc(sizeof(int)))) {
                LOG_CRITICAL;
                kill(pid, SIGUSR1);
            }
            *t_serv = t_servizio;
            if (lpush(cassa->t_clienti_serviti, t_serv) != 0) {
                LOG_CRITICAL;
                kill(pid, SIGUSR1);
            }

            // servo cliente
            msleep(t_servizio);
            set_stato_cliente(cliente, FINITO_CASSA);
            cassa->num_clienti_serviti++;
        } else {
            set_stato_cliente(cliente, USCITA_SEGNALE);
        }
    }
    add_tapertura(cassa, tstart_apertura);
}

/**
 * invio notifica al direttore e lo "sveglio"
*/
static void invia_notifica(cassa_opt_t* cassa, struct timespec* tstart_notifica) {
    int size = get_size(cassa->coda);
    if (size == -1) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }

    if (mutex_lock(cassa->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    *(cassa->notify_size) = size;
    *(cassa->notify_sent) = true;
    if (cond_signal(cassa->notify_cond) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    if (mutex_unlock(cassa->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }

    /* per valutare il tempo che intercorre tra una notifica ed un'altra */
    struct timespec tend_notifica = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tend_notifica);
    LOG_DEBUG("cassa %d - notify sent after %.5f seconds", cassa->id_cassa,
              spec_difftime(*tstart_notifica, tend_notifica));
    *tstart_notifica = tend_notifica;
}