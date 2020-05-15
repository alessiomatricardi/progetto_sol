#define _POSIX_C_SOURCE 200112L
#include <cliente.h>
#include <time.h>
#include <util.h>

void* cliente(void* arg) {
    cliente_opt_t* cliente = (cliente_opt_t*)arg;
    cliente->stato_cliente = ENTRATO;
    msleep(cliente->tempo_acquisti);
    int cassa_scelta = 0;
    if (cliente->num_prodotti > 0) {
        while (1) {
            // scegli cassa tra quelle attive
            if (mutex_lock(cliente->mutex) != 0) {
                perror("cliente mutex lock 2");
                // gestire errore
            }
            cassa_scelta = 0;//scegli_cassa();
            if(cassa_scelta != -1) {
                // mettiti in coda
                if (push(cliente->coda_casse[cassa_scelta], cliente) == -1)
                    ;
            }
            if (mutex_unlock(cliente->mutex) != 0) {
                perror("cliente mutex lock 2");
                // gestire errore
            }
            if(cassa_scelta == -1) {
                cliente->stato_cliente = USCITA_SEGNALE;
                break;
            }

            // fai la wait
            if (mutex_lock(cliente->mutex_cliente) != 0) {
                perror("cliente mutex lock 2");
                // gestire errore
            }
            while (cliente->stato_cliente == ENTRATO) {
                if (cond_wait(cliente->cond_incoda, cliente->mutex_cliente) != 0) {
                    perror("cliente cond wait 1");
                    // GESTIRE ERRORE
                }
            }
            if (cliente->stato_cliente != CAMBIA_CASSA) {
                if (mutex_unlock(cliente->mutex_cliente) != 0) {
                    perror("cliente mutex unlock 2");
                    // gestire errore
                }
                break;
            }
            if (mutex_unlock(cliente->mutex_cliente) != 0) {
                perror("cliente mutex unlock 2");
                // gestire errore
            }
        }
    } else {
        // chiedi a direttore di uscire
        // fai la wait
        if (mutex_lock(cliente->mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
        // needs while
        if (cond_wait(cliente->cond_auth, cliente->mutex) != 0) {
            perror("cliente cond wait 2");
            // GESTIRE ERRORE
        }
        if (mutex_unlock(cliente->mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
    }
    printf("cliente\n");
    // devo dire al supermercato che sono uscito
    // uscire
    pthread_exit((void*)0);
}