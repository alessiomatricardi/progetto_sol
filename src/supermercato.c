#define _POSIX_C_SOURCE 200112L
#include <cassa.h>
#include <cliente.h>
#include <direttore.h>
#include <parsing.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

/* macros */

#define MAX_CONFIG_FILENAME 256
#define DEFAULT_CONFIG_FILENAME "config.txt"
#define ACCEPTABLE_OPTIONS ":c:"

/* funzione di lettura dei parametri passati da terminale */
int set_config_filename(char* config_filename, int argc, char** argv) {
    int opt;
    if ((opt = getopt(argc, argv, ACCEPTABLE_OPTIONS)) != -1) {
        switch (opt) {
            case 'c':
                if (sscanf(optarg, "%255[^\n]", config_filename) != 1) {
                    fprintf(stderr, "Lettura filename fallita\n");
                    return -1;
                }
                return 0;
            default:
                fprintf(stderr, "Opzione -%c non valida\n", (char)optopt);
                return -1;
        }
    }
    return -1;
}

int main(int argc, char** argv) {
    if (argc != 3 && argc != 1) {
        printf("Usage: %s -c <config file>\n", argv[0]);
        printf("or use default: %s [-c config.txt]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    /* set file name del config */
    char config_filename[MAX_CONFIG_FILENAME] = {'\0'};
    if (argc == 3) {
        if (set_config_filename(config_filename, argc, argv) == -1) {
            return EXIT_FAILURE;
        }
    } else {
        strncpy(config_filename, DEFAULT_CONFIG_FILENAME, strlen(DEFAULT_CONFIG_FILENAME) + 1);
    }
    /* caricamento del file di config in struct dedicata */
    config_t config;
    memset(&config, 0, sizeof(config_t));
    if (parse_config(config_filename, &config) != 0) {
        printf("Qualcosa è andato storto durante la lettura del file di configurazione.\n");
        exit(EXIT_FAILURE);
    }
    printf("casse iniziali %d\nt prodotto %d\nt agg clienti %d\n", config.casse_iniziali, config.t_singolo_prodotto, config.t_agg_clienti);
    printf("t max %d\np max %d\nk max %d\ns1 %d\ns2 %d\nc max %d\ne %d\nlog %s\n", config.t_max, config.p_max, config.k_max, config.s1, config.s2, config.c_max, config.e, config.log_file_name);

    // fa tutto

    /* strutture dati da utilizzare */

    cliente_opt_t clienti_opt[config.c_max];
    cassa_opt_t casse_opt[config.k_max];
    direttore_opt_t direttore_opt;
    BQueue_t code_casse[config.k_max];

    /* thread in gioco nel sistema */

    pthread_t th_clienti[config.c_max];
    pthread_t th_casse[config.k_max];
    pthread_t th_direttore;

    /* mutex e variabili di condizione in gioco nel sistema */
    pthread_mutex_t main_mutex;
    pthread_mutex_t manager_mutex;
    pthread_mutex_t client_mutex[config.c_max];

    pthread_cond_t auth_cond;
    pthread_cond_t cond_casse;
    pthread_cond_t cond_incoda;

    /* creazione thread direttore */
    direttore_opt.stato_direttore = ATTIVO;
    direttore_opt.casse = casse_opt;
    direttore_opt.mutex = &main_mutex;
    direttore_opt.mutex_direttore = &manager_mutex;
    direttore_opt.cond_auth = &auth_cond;
    direttore_opt.k_max = config.k_max;
    direttore_opt.soglia_1 = config.s1;
    direttore_opt.soglia_2 = config.s2;
    /* creazione threads casse */
    for(size_t i = 0; i < config.k_max; i++) {

    }
    /* creazione threads clienti */
    for (size_t i = 0; i < config.k_max; i++) {
    }
    return 0;
}