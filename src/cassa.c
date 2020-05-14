#define _POSIX_C_SOURCE 200112L
#include <cassa.h>
#include <cliente.h>

void* cassa(void* arg) {
    cassa_opt_t cassa = *((cassa_opt_t*)arg);
    int t_notifica = cassa.intervallo_notifica;
    while (1) {
        if (mutex_lock(cassa.mutex) != 0) {
            perror("mutex cassa");
        }
        while (cassa.stato_cassa == CHIUSA) {
            if (cond_wait(cassa.cond, cassa.mutex) != 0) {
                perror("cond");
            }
        }
        if (cassa.stato_cassa != APERTA) break;
        if (mutex_unlock(cassa.mutex) != 0) {
            perror("mutex cassa");
        }
        void* temp_cliente = pop(&cassa.coda);
        if (temp_cliente == END_OF_SERVICE) break;
        cliente_opt_t cliente = *((cliente_opt_t*)temp_cliente);
        
        long t_servizio = cassa.tempo_fisso + cassa.tempo_prodotto * cliente.num_prodotti;
        if (t_servizio >= t_notifica) {
            while (t_servizio >= t_notifica) {
                msleep(t_notifica);
                // invia notifica
                t_servizio -= t_notifica;
                t_notifica = cassa.intervallo_notifica;
            }
            msleep(t_servizio);
            t_notifica = (t_notifica - t_servizio) % cassa.intervallo_notifica;
        } else {
            msleep(t_servizio);
            t_notifica = t_notifica - t_servizio;
        }

        if (mutex_lock(cliente.mutex_cliente) != 0) {
            perror("mutex cassa");
        }
        cliente.stato_cliente = FINITO_CASSA;
        if(cond_signal(cliente.cond_incoda) != 0) {
            perror("signal client");
        }
        if (mutex_unlock(cliente.mutex_cliente) != 0) {
            perror("mutex cassa");
        }
    }
    if (cassa.stato_cassa == CHIUSURA) {
        // FLUSH CLIENTI servili tutti
        printf("Ricevuta chiusura\n");
    } else { /* stato = CHIUSURA_IMMEDIATA */
    }
    if (mutex_unlock(cassa.mutex) != 0) {
        perror("mutex cassa");
    }
}