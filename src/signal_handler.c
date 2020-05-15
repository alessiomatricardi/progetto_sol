#define _POSIX_C_SOURCE 200112L
#include <signal.h>
#include <signal_handler.h>
#include <util.h>

void* signal_handler(void* arg) {
    int sig, error = 0;
    sigset_t sigmask;
    error = sigemptyset(&sigmask);
    error |= sigaddset(&sigmask, SIGHUP);
    error |= sigaddset(&sigmask, SIGQUIT);
    error |= sigaddset(&sigmask, SIGUSR1);
    pthread_sigmask(SIG_SETMASK, &sigmask, NULL);

    if (error) {
        // errore
    } else {
        do {
            error = sigwait(&sigmask, &sig);
            if (error) {
                // errore
            }
            printf("Ho catturato %d\n", sig);
        } while (1);
    }
    return NULL;
}