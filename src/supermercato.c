#define _POSIX_C_SOURCE 200112L
#include <cassa.h>
#include <cliente.h>
#include <direttore.h>
#include <parsing.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <util.h>
#include <signal.h>
#include <signal_handler.h>
#include <logger.h>
#include <errno.h>

/* macros */

#define MAX_CONFIG_FILENAME 256
#define DEFAULT_CONFIG_FILENAME "config.txt"
#define ACCEPTABLE_OPTIONS ":c:"

/* id del processo */
pid_t pid;

/* funzione di lettura dei parametri passati da terminale */
static int set_config_filename(char* config_filename, int argc, char** argv) {
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
    /* maschera segnali */
    int error = 0;
    sigset_t sigmask;
    error = sigemptyset(&sigmask);
    error |= sigaddset(&sigmask, SIGHUP);
    error |= sigaddset(&sigmask, SIGQUIT);
    error |= sigaddset(&sigmask, SIGUSR1);
    if((error |= pthread_sigmask(SIG_SETMASK, &sigmask, NULL)) != 0) {
        LOG_CRITICAL(strerror(errno));
        return EXIT_FAILURE;
    }
    /* salva id del processo */
    pid = getpid();

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
    /*
     * printf("casse iniziali %d\nt prodotto %d\nt agg clienti %d\n", config.casse_iniziali, config.t_singolo_prodotto, config.t_agg_clienti);
     * printf("t max %d\np max %d\nk max %d\ns1 %d\ns2 %d\nc max %d\ne %d\nlog %s\n", config.t_max, config.p_max, config.k_tot, config.s1, config.s2, config.c_max, config.e, config.log_file_name);
    */

    // fa tutto

    /* strutture dati da utilizzare */

    sig_handler_opt_t sig_hand_opt;
    cliente_opt_t clienti_opt[config.c_max];
    cassa_opt_t casse_opt[config.k_tot];
    direttore_opt_t direttore_opt;
    BQueue_t* code_casse[config.k_tot];
    cassa_state_t stato_casse[config.k_tot];
    supermercato_state_t stato_supermercato = ATTIVO;
    /* id cliente progressivo */
    int id_cliente = 0;

    /* thread in gioco nel sistema */

    pthread_t th_clienti[config.c_max];
    pthread_t th_casse[config.k_tot];
    pthread_t th_direttore;
    pthread_t th_signal_handler;

    /* mutex e variabili di condizione in gioco nel sistema */
    pthread_mutex_t main_mutex;
    pthread_mutex_t quit_mutex;
    pthread_mutex_t exit_mutex;
    pthread_mutex_t client_mutex[config.c_max];

    pthread_cond_t auth_cond;
    pthread_cond_t cond_casse[config.k_tot];
    pthread_cond_t cond_incoda[config.c_max];

    /* inizializzazione mutex e variabili di condizione */
    /* attributi per ora nulli */
    pthread_mutex_init(&main_mutex, NULL);
    pthread_mutex_init(&quit_mutex, NULL);
    pthread_mutex_init(&exit_mutex, NULL);
    for (size_t i = 0; i < config.c_max; i++) {
        pthread_mutex_init(&client_mutex[i], NULL);
    }

    pthread_cond_init(&auth_cond, NULL);
    for (size_t i = 0; i < config.k_tot; i++) {
        pthread_cond_init(&cond_casse[i], NULL);
    }
    for (size_t i = 0; i < config.c_max; i++) {
        pthread_cond_init(&cond_incoda[i], NULL);
    }

    /* inizializzazione code e stato casse */
    for (size_t i = 0; i < config.k_tot; i++) {
        code_casse[i] = init_BQueue(config.c_max);
    }

    int casse_attive = config.casse_iniziali;
    bool auth_array[config.c_max];
    bool exit_array[config.c_max];
    for(size_t i = 0; i < config.c_max; i++) {
        auth_array[i] = exit_array[i] = false;
    }
    int exited_clients = 0;
    int queue_notify[config.k_tot];
    for (size_t i = 0; i < config.k_tot; i++) {
        queue_notify[i] = 0;
    }

    sig_hand_opt.quit_mutex = &quit_mutex;
    sig_hand_opt.stato_supermercato = &stato_supermercato;

    pthread_create(&th_signal_handler, NULL, signal_handler, &sig_hand_opt);

    /* creazione thread direttore */
    direttore_opt.stato_direttore = &stato_supermercato;
    direttore_opt.casse = casse_opt;
    direttore_opt.main_mutex = &main_mutex;
    direttore_opt.quit_mutex = &quit_mutex;
    direttore_opt.auth_cond = &auth_cond;
    direttore_opt.auth_array = auth_array;
    direttore_opt.num_clienti = config.c_max;
    direttore_opt.casse_tot = config.k_tot;
    direttore_opt.casse_attive = &casse_attive;
    direttore_opt.queue_notify = queue_notify;
    direttore_opt.soglia_1 = config.s1;
    direttore_opt.soglia_2 = config.s2;
    /* attributi per ora nulli */
    pthread_create(&th_direttore, NULL, direttore, &direttore_opt);

    /* creazione threads casse */
    unsigned seed = time(NULL);
    for (size_t i = 0; i < config.k_tot; i++) {
        stato_casse[i] = (i < config.casse_iniziali ? APERTA : CHIUSA);
        casse_opt[i].stato_cassa = &stato_casse[i];
        casse_opt[i].coda = code_casse[i];
        casse_opt[i].main_mutex = &main_mutex;
        casse_opt[i].cond = &cond_casse[i];
        casse_opt[i].queue_size_notify = &queue_notify[i];
        int t_fisso = rand_r(&seed) % (MAX_TF_CASSA - MIN_TF_CASSA + 1) + MIN_TF_CASSA;
        casse_opt[i].tempo_fisso = t_fisso;
        casse_opt[i].tempo_prodotto = config.t_singolo_prodotto;
        casse_opt[i].intervallo_notifica = config.t_agg_clienti;

        pthread_create(&th_casse[i], NULL, cassa, &casse_opt[i]);
    }

    /* creazione threads clienti */
    for (size_t i = 0; i < config.c_max; i++) {
        clienti_opt[i].id_cliente = id_cliente++;
        clienti_opt[i].mutex_cliente = &client_mutex[i];
        clienti_opt[i].stato_cliente = ENTRATO;
        clienti_opt[i].cond_incoda = &cond_incoda[i];
        clienti_opt[i].main_mutex = &main_mutex;
        clienti_opt[i].is_authorized = &auth_array[i];
        clienti_opt[i].auth_cond = &auth_cond;
        clienti_opt[i].stato_casse = stato_casse;
        clienti_opt[i].coda_casse = code_casse;
        clienti_opt[i].casse_attive = &casse_attive;
        clienti_opt[i].exit_mutex = &exit_mutex;
        clienti_opt[i].is_exited = &exit_array[i];
        clienti_opt[i].num_exited = &exited_clients;
        clienti_opt[i].casse_tot = config.k_tot;
        int n_prod = rand_r(&seed) % (config.p_max + 1);
        int t_acquisti = rand_r(&seed) % (config.t_max - MIN_T_ACQUISTI + 1) + MIN_T_ACQUISTI;
        clienti_opt[i].num_prodotti = n_prod;
        clienti_opt[i].tempo_acquisti = t_acquisti;
        clienti_opt[i].seed = seed;

        pthread_create(&th_clienti[i], NULL, cliente, &clienti_opt[i]);
    }

    while(1) {
        if (mutex_lock(&quit_mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
        if(stato_supermercato != ATTIVO) {
            if (mutex_unlock(&quit_mutex) != 0) {
                perror("cliente mutex lock 2");
                // gestire errore
            }
            break;
        }
        if (mutex_unlock(&quit_mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
        if (mutex_lock(&exit_mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
        if (exited_clients >= config.e) {
            // ricicla threads
            for (size_t i = 0; i < config.c_max; i++) {
                if(*(clienti_opt[i].is_exited)) {
                    clienti_opt[i].id_cliente = id_cliente++;
                    clienti_opt[i].stato_cliente = ENTRATO;
                    *(clienti_opt[i].is_exited) = false;
                    clienti_opt[i].num_exited = &exited_clients;
                    int n_prod = rand_r(&seed) % (config.p_max + 1);
                    int t_acquisti = rand_r(&seed) % (config.t_max - MIN_T_ACQUISTI + 1) + MIN_T_ACQUISTI;
                    clienti_opt[i].num_prodotti = n_prod;
                    clienti_opt[i].tempo_acquisti = t_acquisti;

                    pthread_create(&th_clienti[i], NULL, cliente, &clienti_opt[i]);
                }
            }
        }
        if (mutex_unlock(&exit_mutex) != 0) {
            perror("cliente mutex lock 2");
            // gestire errore
        }
    }

    for (size_t i = 0; i < config.c_max; i++) {
        pthread_join(th_clienti[i], NULL);
    }
    for (size_t i = 0; i < config.k_tot; i++) {
        pthread_join(th_casse[i], NULL);
    }
    pthread_join(th_direttore, NULL);
    //pthread_join(th_signal_handler, NULL);

    return 0;
}