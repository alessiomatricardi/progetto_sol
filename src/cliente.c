#define _POSIX_C_SOURCE 200112L
#include <cliente.h>
#include <time.h>
#include <util.h>

#define CASSA_NOT_FOUND -1

void vai_in_coda(cliente_opt_t* cliente, int * scelta, unsigned * seed);
void attendi_turno(cliente_opt_t* cliente);
void uscita_senza_acquisti(cliente_opt_t* cliente);
void avverti_supermercato(cliente_opt_t* cliente);

void* cliente(void* arg) {
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
            printf("il cliente ha scelto la cassa %d\n",cassa_scelta);
            
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
    printf("cliente esce\n");
    // devo dire al supermercato che sono uscito

    // uscire
    //pthread_exit((void*)stato);
    pthread_exit((void*)0);
}

void vai_in_coda(cliente_opt_t* cliente, int * scelta, unsigned * seed) {
    if (mutex_lock(cliente->main_mutex) != 0) {
        perror("cliente mutex lock 2");
        // gestire errore
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
        // gestire errore
    }
}

void attendi_turno(cliente_opt_t* cliente) {
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
    if (mutex_unlock(cliente->mutex_cliente) != 0) {
        perror("cliente mutex unlock 2");
        // gestire errore
    }
}

void uscita_senza_acquisti(cliente_opt_t* cliente) {
    printf("chiedo al direttore\n");
    fflush(stdout);
    // fai la wait
    if (mutex_lock(cliente->main_mutex) != 0) {
        perror("cliente mutex lock 2");
        // gestire errore
    }
    *(cliente->authorized) = false;
    while (!(*(cliente->authorized))){
        if (cond_wait(cliente->cond_auth, cliente->main_mutex) != 0) {
            perror("cliente cond wait 2");
            // GESTIRE ERRORE
        }
    }
    if (mutex_unlock(cliente->main_mutex) != 0) {
        perror("cliente mutex lock 2");
        // gestire errore
    }
}
