#define _POSIX_C_SOURCE 200112L
#include <signal.h>
#include <signal_handler.h>
#include <util.h>
#include <bool.h>
#include <logger.h>

/* pid del processo */
extern pid_t pid;

static bool handle_signal(int signal, sig_handler_opt_t* sig_hand) {
    switch (signal) {
        case SIGHUP: {
            if (mutex_lock(sig_hand->quit_mutex) != 0) {
                perror("cliente mutex lock 2");
                // gestire errore
            }
            *(sig_hand->stato_supermercato) = CHIUSURA;
            if (mutex_unlock(sig_hand->quit_mutex) != 0) {
                perror("cliente mutex lock 2");
                // gestire errore
            }
            return true;
        }
        case SIGQUIT: {
            if (mutex_lock(sig_hand->quit_mutex) != 0) {
                perror("cliente mutex lock 2");
                // gestire errore
            }
            *(sig_hand->stato_supermercato) = CHIUSURA;
            if (mutex_unlock(sig_hand->quit_mutex) != 0) {
                perror("cliente mutex lock 2");
                // gestire errore
            }
            return true;
        }
        case SIGUSR1:
            // fai qualcosa che ammazza tutto
            return true;
        default: return true;
    }
}

void* signal_handler(void* arg) {
    sig_handler_opt_t* sig_hand = (sig_handler_opt_t*)arg;
    /* maschera segnali */
    int sig, error = 0;
    sigset_t sigmask;
    error = sigemptyset(&sigmask);
    error |= sigaddset(&sigmask, SIGHUP);
    error |= sigaddset(&sigmask, SIGQUIT);
    error |= sigaddset(&sigmask, SIGUSR1);
    if ((error |= pthread_sigmask(SIG_SETMASK, &sigmask, NULL)) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }

    if (error) {
        // errore
    } else {
        do {
            error = sigwait(&sigmask, &sig);
            if (error) {
                // errore
            }
            printf("Ho catturato %d\n", sig);
        } while (handle_signal(sig, sig_hand));
    }
    return NULL;
}