#include <cassa.h>
#include <cliente.h>
#include <errno.h>
#include <filestat.h>
#include <logger.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int write_pid(pid_t pid) {
    FILE* f;
    if (mkdir(VAR_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0 && errno != EEXIST) return -1;
    if (mkdir(VAR_RUN_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0 && errno != EEXIST) return -1;
    if ((f = fopen(PID_FILENAME, "w")) == NULL) {
        return -1;
    }
    fprintf(f, "%d\n", pid);
    if (fclose(f) == EOF) return -1;
    return 0;
}

int write_log_filename(const char* log_filename) {
    FILE* f;
    if (mkdir(VAR_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0 && errno != EEXIST) return -1;
    if (mkdir(VAR_RUN_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0 && errno != EEXIST) return -1;
    if ((f = fopen(LOG_FILENAME, "w")) == NULL) {
        return -1;
    }
    fprintf(f, "%s\n", log_filename);
    if (fclose(f) == EOF) return -1;
    return 0;
}

int print_to_log(int num_clienti, Queue_t* clienti, cassa_opt_t* casse, int k, const char* filename) {
    errno = 0;
    FILE* file = NULL;
    if ((file = fopen(filename, "w")) == NULL) {
        LOG_CRITICAL;
        return -1;
    }
    long num_prodotti = 0;
    /** 
     * dati relativi ai clienti
     * FORMAT
     * CLIENTE;ID;TEMPO PERMANENZA;TEMPO ATTESA CODA;# CAMBI CODA;# PRODOTTI ACQUISTATI;\n
    */
    void* tmp_cliente = NULL;
    while ((tmp_cliente = lpop(clienti)) != NULL) {
        cliente_opt_t* c = (cliente_opt_t*)tmp_cliente;
        fprintf(file, "CLIENTE;%d;%.8f;%.8f;%c;%d;%d;\n",
                c->id_cliente, c->t_permanenza,
                spec_difftime(c->tstart_attesa_coda, c->tend_attesa_coda),
                c->num_cambi_coda > 0 ? 'y' : 'n',c->num_cambi_coda, c->num_prodotti);
        free(c);
    }
    if (errno) {
        LOG_CRITICAL;
        fclose(file);
        return -1;
    }
    if (clienti) deleteQueue(clienti, free);
    /** 
     * dati relativi ai cassieri
     * FORMAT
     * CASSA;ID;# CLIENTI SERVITI;# PRODOTTI ELABORATI;# CHIUSURE;\n
     * [TEMPO DI APERTURA i;\n]
     * SERVIZIO\n
     * [TEMPO DI SERVIZIO j;\n]
     * FINE CASSA\n
    */
    for (size_t i = 0; i < k; i++) {
        num_prodotti += casse[i].num_prodotti_elaborati;
        fprintf(file, "CASSA;%d;%d;%ld;%d;\n", casse[i].id_cassa, casse[i].num_clienti_serviti, casse[i].num_prodotti_elaborati, casse[i].num_chiusure);
        void* t = NULL;
        while ((t = lpop(casse[i].tempi_apertura)) != NULL) {
            fprintf(file, "%.8f;\n",*((double*)t));
            free(t);
        }
        if (errno) {
            LOG_CRITICAL;
            fclose(file);
            return -1;
        }
        fprintf(file, "SERVIZIO\n");
        while ((t = lpop(casse[i].t_clienti_serviti)) != NULL) {
            fprintf(file, "%d;\n", *((int*)t));
            free(t);
        }
        if (errno) {
            LOG_CRITICAL;
            fclose(file);
            return -1;
        }
        fprintf(file, "FINE\n");
        if(casse[i].tempi_apertura) deleteQueue(casse[i].tempi_apertura, free);
        if (casse[i].t_clienti_serviti) deleteQueue(casse[i].t_clienti_serviti, free);

    }
    /* scrivo dati relativi a supermercato */
    fprintf(file, "SUPERMERCATO;%d;%ld;\n", num_clienti, num_prodotti);
    if (fclose(file) == EOF) {
        LOG_CRITICAL;
        return -1;
    }
    return 0;
}