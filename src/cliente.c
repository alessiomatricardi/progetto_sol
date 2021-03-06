#define _POSIX_C_SOURCE 200112L
#include <cliente.h>
#include <logger.h>
#include <signal.h>
#include <time.h>
#include <util.h>

#define CASSA_NOT_FOUND -1

/* pid del processo */
extern pid_t pid;

/* il cliente ha bisogno dell'autorizzazione? */
extern volatile int need_auth;

static void set_stato_cliente(cliente_opt_t* cliente, cliente_state_t stato);
static cliente_state_t get_stato_cliente(cliente_opt_t* cliente);
static void vai_in_coda(cliente_opt_t* cliente, int* scelta, unsigned* seed);
static void attendi_turno(cliente_opt_t* cliente);
static void uscita_senza_acquisti(cliente_opt_t* cliente);
static void avverti_supermercato(cliente_opt_t* cliente);

void* fun_cliente(void* arg) {
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
        LOG_CRITICAL(strerror(errno));
        kill(pid, SIGUSR1);
    }

    cliente_opt_t* cliente = (cliente_opt_t*)arg;

    struct timespec tstart_permanenza = {0, 0};
    struct timespec tend_permanenza = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &tstart_permanenza);

    /* fai acquisti */
    msleep(cliente->tempo_acquisti);
    int cassa_scelta = CASSA_NOT_FOUND;
    unsigned seed = cliente->seed;

    if (cliente->num_prodotti > 0) {
        clock_gettime(CLOCK_MONOTONIC, &cliente->tstart_attesa_coda);
        while (1) {
            // scegli cassa tra quelle attive
            vai_in_coda(cliente, &cassa_scelta, &seed);

            if (cassa_scelta == CASSA_NOT_FOUND) {
                set_stato_cliente(cliente, USCITA_SEGNALE);
                break;
            }

            // fai la wait
            attendi_turno(cliente);

            /**
             * se deve cambiare cassa, torna a scegliere la cassa
             * altrimenti può uscire dal supermercato
            */
            if (get_stato_cliente(cliente) != CAMBIA_CASSA)
                break;
            else {
                cliente->num_cambi_coda++;
                LOG_DEBUG("cliente %d cambia cassa", cliente->id_cliente);
            }
        }
    } else {
        // chiedi a direttore di uscire
        LOG_DEBUG("cliente %d vorrebbe uscire senza acquisti", cliente->id_cliente);
        uscita_senza_acquisti(cliente);
        LOG_DEBUG("cliente %d esce senza acquisti", cliente->id_cliente);
    }
    LOG_DEBUG("cliente %d esce con stato %d", cliente->id_cliente, get_stato_cliente(cliente));
    clock_gettime(CLOCK_MONOTONIC, &tend_permanenza);
    cliente->t_permanenza = spec_difftime(tstart_permanenza, tend_permanenza);
    // devo dire al supermercato che sono uscito
    avverti_supermercato(cliente);
    // uscire
    return NULL;
}

/**
 * modifica lo stato del cliente
*/
static void set_stato_cliente(cliente_opt_t* cliente, cliente_state_t stato) {
    if (mutex_lock(cliente->mutex_cliente) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    cliente->stato_cliente = stato;
    if (mutex_unlock(cliente->mutex_cliente) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}

/**
 * restituisce lo stato del cliente
*/
static cliente_state_t get_stato_cliente(cliente_opt_t* cliente) {
    cliente_state_t stato = ENTRATO;
    if (mutex_lock(cliente->mutex_cliente) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    stato = cliente->stato_cliente;
    if (mutex_unlock(cliente->mutex_cliente) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    return stato;
}

/**
 * il cliente si mette in coda in una delle casse attive (in modo random)
 * la scelta viene salvata nel parametro scelta
*/
static void vai_in_coda(cliente_opt_t* cliente, int* scelta, unsigned* seed) {
    if (mutex_lock(cliente->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    if (*(cliente->num_casse_attive) > 0) {
        /*
         * scelgo randomicamente una delle casse attive in quel momento
         * tmp compreso tra 0 e num_casse_attive-1
        */
        int tmp = rand_r(seed) % *(cliente->num_casse_attive);
        for (size_t i = 0; i < cliente->num_casse_tot; i++) {
            if (cliente->stato_casse[i] != CHIUSA) {
                if (tmp == 0) {
                    // mettiti in coda
                    set_stato_cliente(cliente, IN_CODA);
                    if (push(cliente->coda_casse[i], cliente) == -1) {
                        LOG_CRITICAL;
                        kill(pid, SIGUSR1);
                    }
                    *scelta = i;
                    LOG_DEBUG("cliente %d in cassa %d", cliente->id_cliente, *scelta);
                    break;
                } else {
                    tmp--;
                }
            }
        }
    } else
        *scelta = CASSA_NOT_FOUND;
    if (mutex_unlock(cliente->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}

/**
 * il cliente si mette in attesa dopo essere entrato nella coda
*/
static void attendi_turno(cliente_opt_t* cliente) {
    if (mutex_lock(cliente->mutex_cliente) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    while (cliente->stato_cliente == IN_CODA) {
        if (cond_wait(cliente->cond_incoda, cliente->mutex_cliente) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
    }
    if (mutex_unlock(cliente->mutex_cliente) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}

/**
 * se il supermercato non sta chiudendo immediatamente (sigquit),
 * allora il cliente attende l'autorizzazione da parte del direttore per uscire.
*/
static void uscita_senza_acquisti(cliente_opt_t* cliente) {
    set_stato_cliente(cliente, USCITA_SENZA_ACQUISTI);
    if (mutex_lock(cliente->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    *(cliente->is_authorized) = false;
    while (!(*(cliente->is_authorized)) && need_auth) {
        if (cond_wait(cliente->auth_cond, cliente->main_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
    }
    if (mutex_unlock(cliente->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}

/**
 * il cliente avverte il supermercato di essere uscito
*/
static void avverti_supermercato(cliente_opt_t* cliente) {
    if (mutex_lock(cliente->exit_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    *(cliente->is_exited) = true;
    *(cliente->num_exited) += 1;
    LOG_DEBUG("clienti usciti %d", *(cliente->num_exited));
    if (cond_signal(cliente->exit_cond) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    if (mutex_unlock(cliente->exit_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}