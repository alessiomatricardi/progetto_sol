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

/* per informare i cassieri quando è il momento di chiudere definitivamente */
extern volatile sig_atomic_t cashier_should_quit;

static void add_abs_time(struct timespec* ts, int msec) {
    clock_gettime(CLOCK_MONOTONIC, ts);
    if (msec <= 0) return;
    ts->tv_sec += msec / 1000;
    ts->tv_nsec += (msec % 1000) * MILLION;
    if (ts->tv_nsec >= BILLION) {
        ts->tv_sec += (ts->tv_nsec / BILLION);
        ts->tv_nsec -= BILLION;
    }
}
static bool
check_apertura(cassa_opt_t* cassa, cassa_state_t* stato, struct timespec* tstart_notifica, int* t_not);
static void set_stato_cliente(cliente_opt_t* cliente, cliente_state_t stato_cliente);
static void libera_cassa(cassa_opt_t* cassa);
static void chiusura_definitiva(cassa_opt_t* cassa, cassa_state_t stato);
static void invia_notifica(cassa_opt_t* cassa, struct timespec* tstart_notifica);

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
    cassa_state_t* stato = cassa->stato_cassa;

    bool res = true;
    int t_notifica = cassa->intervallo_notifica;
    void* temp_cliente = NULL;

    /* per valutare il tempo che intercorre tra una notifica ed un'altra */
    struct timespec tstart_notifica = {0, 0};
    struct timespec ts_notifica = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tstart_notifica);
    int* t_serv = NULL;

    while (1) {
        /* controllo di essere aperta */
        res = check_apertura(cassa, stato, &tstart_notifica, &t_notifica);
        if (!res) break;

        /* devo servire un cliente */
        add_abs_time(&ts_notifica, t_notifica);
        temp_cliente = pop(cassa->coda, &ts_notifica);
        if (CHECK_NULL(temp_cliente)) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }

        if (temp_cliente == NOMORECLIENTS) {
            // invia notifica
            invia_notifica(cassa, &tstart_notifica);
            t_notifica = cassa->intervallo_notifica;
            continue;
        }
        cliente_opt_t* cliente = (cliente_opt_t*)temp_cliente;

        /* ricalcolo t_notifica */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double msec_diff = SEC_TO_MSEC * spec_difftime(now, ts_notifica);
        t_notifica = (int)msec_diff + ((msec_diff - (int)msec_diff) > 0);

        int t_servizio = cassa->tempo_fisso + cassa->tempo_prodotto * cliente->num_prodotti;
        if (CHECK_NULL(t_serv = malloc(sizeof(int)))) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        *t_serv = t_servizio;
        if (lpush(cassa->t_clienti_serviti, t_serv) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        while (t_servizio >= t_notifica) {
            msleep(t_notifica);

            // invia notifica
            invia_notifica(cassa, &tstart_notifica);

            t_servizio -= t_notifica;
            t_notifica = cassa->intervallo_notifica;
        }
        msleep(t_servizio);
        t_notifica -= t_servizio;
        set_stato_cliente(cliente, FINITO_CASSA);
        cassa->num_clienti_serviti++;
    }
    LOG_DEBUG("cassa %d - chiusura ricevuta", cassa->id_cassa);
    chiusura_definitiva(cassa, *stato);
    LOG_DEBUG("cassa %d - chiude definitivamente", cassa->id_cassa);
    return NULL;
}

static bool
check_apertura(cassa_opt_t* cassa, cassa_state_t* stato, struct timespec* tstart_notifica, int* t_notifica) {
    struct timespec tstart_apertura = {0, 0}, tend_apertura = {0, 0};
    if (mutex_lock(cassa->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    while (*stato == CHIUSA) {
        libera_cassa(cassa);
        cassa->num_chiusure++;
        clock_gettime(CLOCK_MONOTONIC, &tend_apertura);
        double *t_apertura;
        if (CHECK_NULL(t_apertura = malloc(sizeof(double)))) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        *t_apertura = spec_difftime(tstart_apertura, tend_apertura);
        if (lpush(cassa->tempi_apertura, t_apertura) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        if (cond_wait(cassa->cond, cassa->main_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        *t_notifica = cassa->intervallo_notifica;
        clock_gettime(CLOCK_MONOTONIC, tstart_notifica);
    }
    if (*stato != APERTA) { /* ricevuto segnale */
        if (mutex_unlock(cassa->main_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        clock_gettime(CLOCK_MONOTONIC, &tend_apertura);
        return false;
    }
    clock_gettime(CLOCK_MONOTONIC, &tstart_apertura);
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
        continue;
    }
}

static void chiusura_definitiva(cassa_opt_t* cassa, cassa_state_t stato) {
    if (stato == TERMINA) return;
    void* tmp_cliente;
    struct timespec ts = {0, 0};
    while (!cashier_should_quit) {
        add_abs_time(&ts, 100);
        tmp_cliente = pop(cassa->coda, &ts);
        if (CHECK_NULL(tmp_cliente)) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        if (tmp_cliente == NOMORECLIENTS) continue;
        cliente_opt_t* cliente = (cliente_opt_t*)tmp_cliente;
        if (stato == SERVI_E_TERMINA) {
            int t_servizio = cassa->tempo_fisso + cassa->tempo_prodotto * cliente->num_prodotti;
            msleep(t_servizio);
            set_stato_cliente(cliente, FINITO_CASSA);
            cassa->num_clienti_serviti++;
        } else {
            set_stato_cliente(cliente, USCITA_SEGNALE);
        }
    }
}

static void invia_notifica(cassa_opt_t* cassa, struct timespec* tstart_notifica) {
    int size = get_size(cassa->coda);
    if (size == -1) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    if (mutex_lock(cassa->notify_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    *(cassa->queue_size_notify) = size;
    struct timespec tend_notifica = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tend_notifica);
    /* per valutare il tempo che intercorre tra una notifica ed un'altra */
    LOG_DEBUG("cassa %d - notify sent after %.5f seconds", cassa->id_cassa,
              spec_difftime(*tstart_notifica, tend_notifica));
    *tstart_notifica = tend_notifica;
    if (mutex_unlock(cassa->notify_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}