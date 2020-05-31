#define _POSIX_C_SOURCE 200112L
#include <direttore.h>
#include <logger.h>
#include <string.h>
#include <util.h>

#define UPDATE_SECONDS 4
#define AUTH_WAIT_MSEC 200

/* pid del processo */
extern pid_t pid;

/* per informare cassieri e direttore quando è il momento di chiudere definitivamente */
extern volatile int should_quit;

static bool check_apertura(direttore_opt_t* direttore);
static void controlla_casse(direttore_opt_t* direttore, struct timespec* update_time);
static void chiudi_casse(direttore_opt_t* direttore);
static void attendi_casse(direttore_opt_t* direttore);
static void autorizza_uscita(direttore_opt_t* direttore);

void* fun_direttore(void* arg) {
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

    direttore_opt_t* direttore = (direttore_opt_t*)arg;

    bool res = false;
    struct timespec update_time[direttore->num_casse_tot]; // tempo di ultimo aggiornamento stato della cassa
    memset(update_time, 0, sizeof(update_time));

    /* creazione threads casse */
    for (size_t i = 0; i < direttore->num_casse_tot; i++) {
        if (i < *(direttore->num_casse_attive))
            pthread_create(&direttore->th_casse[i], &direttore->attr_casse[i], fun_cassa, &direttore->casse[i]);
    }

    while (1) {
        // controlla stato
        res = check_apertura(direttore);
        if (!res) break;

        // controlla notifiche, se necessario apri/chiudi cassa
        controlla_casse(direttore, update_time);

        // risveglia clienti che attendono autorizzazione per uscire
        autorizza_uscita(direttore);
    }
    /* chiudi tutte le casse */
    chiudi_casse(direttore);
    while (!should_quit) {
        msleep(AUTH_WAIT_MSEC); // attesa passiva
        // risveglia eventuali clienti che attendono autorizzazione per uscire
        autorizza_uscita(direttore);
    }
    attendi_casse(direttore);

    return NULL;
}

/**
 * controlla di non aver ricevuto alcun segnale di chiusura
*/
static bool check_apertura(direttore_opt_t* direttore) {
    direttore_state_t* stato = direttore->stato_direttore;
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

/**
 * gestione delle casse
*/
static void controlla_casse(direttore_opt_t* direttore, struct timespec* update_time) {
    //if (!*(direttore->casse_partite)) return;
    /* chiude una cassa se ci sono almeno SOGLIA1 casse che hanno al più un cliente */
    int soglia1 = direttore->soglia_1;
    /* apre una cassa (se possibile) se c’è almeno una cassa con almeno SOGLIA2 clienti in coda */
    int soglia2 = direttore->soglia_2;

    int* queue_notify = direttore->queue_notify;
    int count_s1 = 0;
    bool is_verified_soglia1 = false;
    bool is_verified_soglia2 = false;
    int to_open_index = -1;
    int to_close_index = -1;
    int to_close_num_clienti = direttore->num_clienti;
    struct timespec now = {0, 0};
    bool check_casse;

    if (mutex_lock(direttore->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    // controllo che tutte le casse attualmente aperte mi abbiano notificato
    do {
        check_casse = true;
        size_t i = 0;
        while(i < direttore->num_casse_tot && check_casse) {
            if (*(direttore->casse[i].stato_cassa) == APERTA && !direttore->notify_sent[i]) {
                check_casse = false;
            }
            i++;
        }
        // se no, faccio attesa passiva fino a che non vengo risvegliato
        if (!check_casse) {
            if (cond_wait(direttore->notify_cond, direttore->main_mutex) != 0) {
                LOG_CRITICAL;
                kill(pid, SIGUSR1);
            }
        }
    } while (!check_casse);
    // reset array "ho notificato"
    for (size_t i = 0; i < direttore->num_casse_tot; i++) {
        direttore->notify_sent[i] = false;
    }

    /**
     * algoritmo del direttore
     * 
     * se posso aprire una cassa:
     * - apro la cassa chiusa da più tempo rispetto alle altre
     * - la apro se e solo se è chiusa da più di UPDATE_SECONDS secondi
     * 
     * se posso chiudere una cassa:
     * - chiudo la cassa con meno clienti
     * - la chiudo se e solo se è aperta da più di UPDATE_SECONDS secondi
     * 
     * la precedenza viene data alla chiusura di una cassa
    */
    clock_gettime(CLOCK_MONOTONIC, &now);
    for (size_t i = 0; i < direttore->num_casse_tot; i++) {
        if (*(direttore->casse[i].stato_cassa) == APERTA) {
            if (queue_notify[i] <= 1) {
                count_s1++;
                if (spec_difftime(update_time[i], now) > UPDATE_SECONDS && queue_notify[i] < to_close_num_clienti) {
                    to_close_num_clienti = queue_notify[i];
                    to_close_index = i;
                }
            } else if (queue_notify[i] >= soglia2) {
                is_verified_soglia2 = true;
            }
        } else {
            if (to_open_index == -1) {
                to_open_index = i;
            } else {
                double diff = spec_difftime(update_time[to_open_index], update_time[i]);
                if (diff < 0) {
                    to_open_index = i;
                }
            }
        }
    }
    if(to_open_index != -1) {
        double elapsed = spec_difftime(update_time[to_open_index], now);
        if(elapsed <= UPDATE_SECONDS) {
            to_open_index = -1;
        }
    }
    is_verified_soglia1 = count_s1 >= soglia1 && to_close_index != -1 && *(direttore->num_casse_attive) > MIN_CASSE_APERTE;

    if (mutex_unlock(direttore->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }

    // chiudo una cassa, se possibile
    if (is_verified_soglia1) {
        if (mutex_lock(direttore->main_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        *(direttore->casse[to_close_index].stato_cassa) = CHIUSA;

        *(direttore->num_casse_attive) -= 1;
        if (mutex_unlock(direttore->main_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        if (pthread_join(direttore->th_casse[to_close_index], NULL) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        update_time[to_close_index] = now;
        LOG_DEBUG("Il direttore ha chiuso la cassa %d", to_close_index);
        return;
    }
    // apro una cassa, se possibile
    else if (is_verified_soglia2 && to_open_index != -1) {
        if (mutex_lock(direttore->main_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        *(direttore->casse[to_open_index].stato_cassa) = APERTA;

        if (pthread_create(&direttore->th_casse[to_open_index], &direttore->attr_casse[to_open_index], fun_cassa, &direttore->casse[to_open_index]) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }

        *(direttore->num_casse_attive) += 1;
        if (mutex_unlock(direttore->main_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        update_time[to_open_index] = now;
        LOG_DEBUG("Il direttore ha aperto la cassa %d", to_open_index);
    }
}

/**
 * indica a tutte le casse attualmente aperte che devono chiudere in sicurezza
*/
static void chiudi_casse(direttore_opt_t* direttore) {
    direttore_state_t* stato = direttore->stato_direttore;
    if (mutex_lock(direttore->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    for (size_t i = 0; i < direttore->num_casse_tot; i++) {
        if (*(direttore->casse[i].stato_cassa) != CHIUSA) {
            *(direttore->casse[i].stato_cassa) = (*stato == CHIUSURA) ? SERVI_E_TERMINA : NON_SERVIRE_E_TERMINA;
        }
    }
    if (mutex_unlock(direttore->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}

/**
 * join delle casse che hanno terminato di servire tutti i clienti
*/
static void attendi_casse(direttore_opt_t* direttore) {
    if (mutex_lock(direttore->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    for (size_t i = 0; i < direttore->num_casse_tot; i++) {
        if (*(direttore->casse[i].stato_cassa) != CHIUSA) {
            if (pthread_join(direttore->th_casse[i], NULL) != 0) {
                LOG_CRITICAL;
                kill(pid, SIGUSR1);
            }
        }
    }
    if (mutex_unlock(direttore->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}

/**
 * autorizza tutti i clienti che attendono di uscire
*/
static void autorizza_uscita(direttore_opt_t* direttore) {
    bool* auth_array = direttore->auth_array;
    if (mutex_lock(direttore->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    for (size_t i = 0; i < direttore->num_clienti; i++) {
        if (!auth_array[i]) auth_array[i] = true;
    }
    if (cond_broadcast(direttore->auth_cond) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    if (mutex_unlock(direttore->main_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}