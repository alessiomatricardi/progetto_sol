#define _POSIX_C_SOURCE 200112L
#include <direttore.h>
#include <logger.h>
#include <string.h>
#include <util.h>

/* pid del processo */
extern pid_t pid;

static bool check_apertura(direttore_opt_t* direttore, direttore_state_t* stato);
static void controlla_casse(direttore_opt_t* direttore, unsigned seed);
static void chiudi_casse(direttore_opt_t* direttore, direttore_state_t* stato);
static void autorizza_uscita(bool* auth_array, int num_clienti, pthread_mutex_t* mtx, pthread_cond_t* cond);

void* direttore(void* arg) {
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

    direttore_opt_t* direttore = (direttore_opt_t*)arg;
    direttore_state_t* stato = direttore->stato_direttore;

    bool res = false;

    unsigned seed = direttore->seed;

    while (1) {
        // controlla stato
        res = check_apertura(direttore, stato);
        if (!res) break;

        // controlla notifiche, se necessario apri/chiudi cassa
        controlla_casse(direttore, seed);

        // risveglia clienti che attendono autorizzazione per uscire
        autorizza_uscita(direttore->auth_array, direttore->num_clienti, direttore->main_mutex, direttore->auth_cond);
    }
    /* chiudi tutte le casse */
    chiudi_casse(direttore, stato);

    pthread_exit((void*)0);
}

static bool check_apertura(direttore_opt_t* direttore, direttore_state_t* stato) {
    if (mutex_lock(direttore->quit_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    if (*stato != ATTIVO) {
        if (mutex_unlock(direttore->quit_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        return false;
    }
    if (mutex_unlock(direttore->quit_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    return true;
}

static void controlla_casse(direttore_opt_t* direttore, unsigned seed) {
    if (!*(direttore->casse_partite)) return;
    int soglia1 = direttore->soglia_1; /* chiude una cassa se ci sono almeno SOGLIA1 casse che hanno al più un cliente */
    int soglia2 = direttore->soglia_2; /* apre una cassa (se possibile) se c’è almeno una cassa con almeno SOGLIA2 clienti in coda */
    int* queue_notify = direttore->queue_notify;
    int count_s1 = 0;
    bool is_verified_soglia1 = false;
    bool is_verified_soglia2 = false;
    int to_open_index = -1;
    int to_close_index = -1;

    if (mutex_lock(direttore->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    int num_casse_chiuse = direttore->casse_tot - *(direttore->num_casse_attive);
    int to_open_rand = num_casse_chiuse > 0 ? rand_r(&seed) % num_casse_chiuse : -1;
    int to_close_rand = rand_r(&seed) % *(direttore->num_casse_attive);

    for (size_t i = 0; i < direttore->casse_tot; i++) {
        if (*(direttore->casse[i].stato_cassa) == APERTA) {
            if (to_close_rand == 0) {
                to_close_index = i;
            }
            to_close_rand--;
            if (queue_notify[i] <= 1) {
                count_s1++;
            } else if (queue_notify[i] >= soglia2) {
                is_verified_soglia2 = true;
            }
        } else {
            if (to_open_rand == 0) {
                to_open_index = i;
            }
            to_open_rand--;
        }
    }

    if (count_s1 >= soglia1) is_verified_soglia1 = true;

    // se entrambe verificate, non apro ne chiudo alcuna cassa
    if (is_verified_soglia1 && is_verified_soglia2) {
        if (mutex_unlock(direttore->main_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        return;
    }
    // chiudo una cassa, se possibile
    if (is_verified_soglia1 && *(direttore->num_casse_attive) > 1) {
        // chiudo l'ultima cassa segnalata da soglia1
        *(direttore->casse[to_close_index].stato_cassa) = CHIUSA;

        *(direttore->num_casse_attive) -= 1;
        LOG_DEBUG("Ha appena chiuso la cassa %d", to_close_index);
    } else if (is_verified_soglia2 && (*(direttore->num_casse_attive) < direttore->casse_tot)) {
        // apro la prima cassa non aperta
        *(direttore->casse[to_open_index].stato_cassa) = APERTA;

        if (cond_signal(direttore->casse[to_open_index].cond) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }

        *(direttore->num_casse_attive) += 1;
        LOG_DEBUG("Ha appena aperto la cassa %d", to_open_index);
    }

    if (mutex_unlock(direttore->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}

static void chiudi_casse(direttore_opt_t* direttore, direttore_state_t* stato) {
    cassa_state_t nuovo_stato_cassa = (*stato == CHIUSURA) ? CHIUSURA_SUPERMERCATO : CHIUSURA_IMMEDIATA_SUPERMERCATO;
    if (mutex_lock(direttore->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    for (size_t i = 0; i < direttore->casse_tot; i++) {
        *(direttore->casse[i].stato_cassa) = nuovo_stato_cassa;

        if (push(direttore->casse[i].coda, END_OF_SERVICE) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }

        if (cond_signal(direttore->casse[i].cond) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
    }
    if (mutex_unlock(direttore->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}

static void autorizza_uscita(bool* auth_array, int num_clienti, pthread_mutex_t* mtx, pthread_cond_t* cond) {
    if (mutex_lock(mtx) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    for (size_t i = 0; i < num_clienti; i++) {
        if (!auth_array[i]) auth_array[i] = true;
    }
    if (cond_broadcast(cond) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    if (mutex_unlock(mtx) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}