#define _POSIX_C_SOURCE 200112L
#include <cliente.h>
#include <time.h>
#include <util.h>
#include <signal.h>
#include <logger.h>

#define CASSA_NOT_FOUND -1

/* pid del processo */
extern pid_t pid;

static void vai_in_coda(cliente_opt_t* cliente, int * scelta, unsigned * seed);
static void attendi_turno(cliente_opt_t* cliente);
static void uscita_senza_acquisti(cliente_opt_t* cliente);
static void avverti_supermercato(cliente_opt_t* cliente);

void* cliente(void* arg) {
    /* da vedere */
    int error = 0;
    sigset_t sigmask;
    error = sigemptyset(&sigmask);
    error |= sigaddset(&sigmask, SIGHUP);
    error |= sigaddset(&sigmask, SIGQUIT);
    error |= sigaddset(&sigmask, SIGUSR1);
    pthread_sigmask(SIG_SETMASK, &sigmask, NULL);
    /* da vedere */

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
            
            if(cassa_scelta == CASSA_NOT_FOUND) {
                *stato = USCITA_SEGNALE;
                break;
            }

            // fai la wait
            attendi_turno(cliente);
            /**
             * se deve cambiare cassa, torna a scegliere la cassa
             * altrimenti può uscire dal supermercato
            */
            if(*stato != CAMBIA_CASSA) break;
        }
    } else {
        // chiedi a direttore di uscire
        uscita_senza_acquisti(cliente);
    }
    LOG_DEBUG("cliente esce");
    // devo dire al supermercato che sono uscito
    avverti_supermercato(cliente);
    // uscire
    //pthread_exit((void*)stato);
    pthread_exit((void*)0);
}

static void vai_in_coda(cliente_opt_t* cliente, int * scelta, unsigned * seed) {
    if (mutex_lock(cliente->main_mutex) != 0) {
        perror("cliente mutex lock 2");
        kill(pid, SIGUSR1);
    }
    if(cliente->casse_attive != 0) {
        /*
         * scelgo randomicamente una delle casse attive in quel momento
         * tmp compreso tra 0 e casse_attive-1
        */
        int tmp = rand_r(seed) % *(cliente->casse_attive);
        for(size_t i = 0; i < cliente->casse_tot; i++) {
            if((cliente->stato_casse[i] == APERTA) && (tmp == 0)) {
                // mettiti in coda
                if (push(cliente->coda_casse[i], cliente) == -1) {
                    // gestisci errore
                }
                *scelta = i;
                break;
            } else tmp--;
        }
    } else *scelta = CASSA_NOT_FOUND;
    if (mutex_unlock(cliente->main_mutex) != 0) {
        perror("cliente mutex lock 2");
        kill(pid, SIGUSR1);
    }
}

static void attendi_turno(cliente_opt_t* cliente) {
    if (mutex_lock(cliente->mutex_cliente) != 0) {
        perror("cliente mutex lock 2");
        kill(pid, SIGUSR1);
    }
    while (cliente->stato_cliente == ENTRATO) {
        if (cond_wait(cliente->cond_incoda, cliente->mutex_cliente) != 0) {
            perror("cliente cond wait 1");
            kill(pid, SIGUSR1);
        }
    }
    if (mutex_unlock(cliente->mutex_cliente) != 0) {
        perror("cliente mutex unlock 2");
        kill(pid, SIGUSR1);
    }
}

static void uscita_senza_acquisti(cliente_opt_t* cliente) {
    printf("chiedo al direttore\n");
    fflush(stdout);
    // fai la wait
    if (mutex_lock(cliente->main_mutex) != 0) {
        perror("cliente mutex lock 2");
        kill(pid, SIGUSR1);
    }
    *(cliente->is_authorized) = false;
    while (!(*(cliente->is_authorized))){
        if (cond_wait(cliente->auth_cond, cliente->main_mutex) != 0) {
            perror("cliente cond wait 2");
            kill(pid, SIGUSR1);
        }
    }
    if (mutex_unlock(cliente->main_mutex) != 0) {
        perror("cliente mutex lock 2");
        kill(pid, SIGUSR1);
    }
}

static void avverti_supermercato(cliente_opt_t* cliente) {
    if (mutex_lock(cliente->exit_mutex) != 0) {
        perror("cliente mutex lock 2");
        kill(pid, SIGUSR1);
    }
    *(cliente->is_exited) = true;
    *(cliente->num_exited) += 1;
    if (mutex_unlock(cliente->exit_mutex) != 0) {
        perror("cliente mutex lock 2");
        kill(pid, SIGUSR1);
    }
}