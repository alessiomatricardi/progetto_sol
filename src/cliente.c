#define _POSIX_C_SOURCE 200112L
#include <cliente.h>
#include <logger.h>
#include <signal.h>
#include <time.h>
#include <util.h>

#define CASSA_NOT_FOUND -1

/* pid del processo */
extern pid_t pid;

static void vai_in_coda(cliente_opt_t* cliente, int* scelta, unsigned* seed);
static void attendi_turno(cliente_opt_t* cliente);
static void uscita_senza_acquisti(cliente_opt_t* cliente);
static void avverti_supermercato(cliente_opt_t* cliente);

void* cliente(void* arg) {
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
    cliente->stato_cliente = ENTRATO;

    /* fai acquisti */
    msleep(cliente->tempo_acquisti);

    cliente_state_t* stato = &(cliente->stato_cliente);
    int cassa_scelta = CASSA_NOT_FOUND;
    unsigned seed = cliente->seed;

    if (cliente->num_prodotti > 0) {
        while (1) {
            // scegli cassa tra quelle attive
            vai_in_coda(cliente, &cassa_scelta, &seed);

            if (cassa_scelta == CASSA_NOT_FOUND) {
                *stato = USCITA_SEGNALE;
                break;
            }

            // fai la wait
            attendi_turno(cliente);

            /**
             * se deve cambiare cassa, torna a scegliere la cassa
             * altrimenti può uscire dal supermercato
            */
            if (*stato != CAMBIA_CASSA) break;
        }
    } else {
        // chiedi a direttore di uscire
        LOG_DEBUG("cliente %d esce senza acquisti", cliente->id_cliente);
        uscita_senza_acquisti(cliente);
    }
    //LOG_DEBUG("cliente %d esce", cliente->id_cliente);
    // devo dire al supermercato che sono uscito
    avverti_supermercato(cliente);
    // uscire
    //pthread_exit((void*)stato);
    pthread_exit((void*)0);
}

/**
 * il cliente si mette in coda in una delle casse attive (in modo random)
 * la scelta viene salvata nel parametro scelta
 * 
 * @param cliente puntatore alla struttura dati del cliente
 * @param scelta variabile dove viene salvata la scelta del cliente
 * @param seed seed per randomizzazione
*/
static void vai_in_coda(cliente_opt_t* cliente, int* scelta, unsigned* seed) {
    if (mutex_lock(cliente->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    if (cliente->num_casse_attive != 0) {
        /*
         * scelgo randomicamente una delle casse attive in quel momento
         * tmp compreso tra 0 e num_casse_attive-1
        */
        int tmp = rand_r(seed) % *(cliente->num_casse_attive);
        for (size_t i = 0; i < cliente->casse_tot; i++) {
            if ((cliente->stato_casse[i] == APERTA) && (tmp == 0)) {
                // mettiti in coda
                if (push(cliente->coda_casse[i], cliente) == -1) {
                    LOG_CRITICAL;
                    kill(pid, SIGUSR1);
                }
                *scelta = i;
                LOG_DEBUG("cliente %d in cassa %d", cliente->id_cliente, *scelta);
                break;
            } else
                tmp--;
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
 * 
 * @param cliente puntatore alla struttura dati del cliente 
*/
static void attendi_turno(cliente_opt_t* cliente) {
    if (mutex_lock(cliente->mutex_cliente) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    while (cliente->stato_cliente == ENTRATO) {
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
 * il cliente attende l'autorizzazione da parte del direttore per uscire
 * 
 * @param cliente puntatore alla struttura dati del cliente
*/
static void uscita_senza_acquisti(cliente_opt_t* cliente) {
    // fai la wait
    if (mutex_lock(cliente->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    while (!(*(cliente->is_authorized))) {
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
 * 
 * @param cliente puntatore alla struttura dati del cliente 
*/
static void avverti_supermercato(cliente_opt_t* cliente) {
    if (mutex_lock(cliente->exit_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    *(cliente->is_exited) = true;
    *(cliente->num_exited) += 1;
    LOG_DEBUG("clienti usciti %d", *(cliente->num_exited));
    if (mutex_unlock(cliente->exit_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}