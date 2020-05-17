#define _POSIX_C_SOURCE 200112L
#include <bool.h>
#include <logger.h>
#include <signal.h>
#include <signal_handler.h>
#include <util.h>

/* pid del processo */
extern pid_t pid;

/* variabile di terminazione del processo supermercato */
extern volatile sig_atomic_t continua;

static bool handle_signal(int signal, sig_handler_opt_t* sig_hand) {
    switch (signal) {
        case SIGHUP: {
            if (mutex_lock(sig_hand->quit_mutex) != 0) {
                LOG_CRITICAL;
                kill(pid, SIGUSR1);
            }
            *(sig_hand->stato_supermercato) = CHIUSURA;
            if (mutex_unlock(sig_hand->quit_mutex) != 0) {
                LOG_CRITICAL;
                kill(pid, SIGUSR1);
            }
            return true;
        }
        case SIGQUIT: {
            if (mutex_lock(sig_hand->quit_mutex) != 0) {
                LOG_CRITICAL;
                kill(pid, SIGUSR1);
            }
            *(sig_hand->stato_supermercato) = CHIUSURA_IMMEDIATA;
            if (mutex_unlock(sig_hand->quit_mutex) != 0) {
                LOG_CRITICAL;
                kill(pid, SIGUSR1);
            }
            return true;
        }
        case SIGUSR1:
            continua = false;
            return false;
        default:
            return false;
    }
}

void* signal_handler(void* arg) {
    /* maschera segnali */
    int sig, error = 0;
    sigset_t sigmask;
    error = sigemptyset(&sigmask);
    error |= sigaddset(&sigmask, SIGHUP);
    error |= sigaddset(&sigmask, SIGQUIT);
    error |= sigaddset(&sigmask, SIGUSR1);
    error |= sigaddset(&sigmask, SIGUSR2);
    if ((error |= pthread_sigmask(SIG_SETMASK, &sigmask, NULL)) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGKILL);
    }

    sig_handler_opt_t* sig_hand = (sig_handler_opt_t*)arg;

    do {
        error = sigwait(&sigmask, &sig);
        if (error) {
            LOG_CRITICAL;
            kill(pid, SIGKILL);
        }
        //printf("Ho catturato %d\n", sig);
    } while (handle_signal(sig, sig_hand));
    
    return NULL;
}