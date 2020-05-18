#define _POSIX_C_SOURCE 200112L
#include <cassa.h>
#include <cliente.h>
#include <direttore.h>
#include <errno.h>
#include <logger.h>
#include <parsing.h>
#include <signal.h>
#include <signal_handler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

/* macros */

#define MAX_CONFIG_FILENAME 256
#define DEFAULT_CONFIG_FILENAME "config.txt"
#define ACCEPTABLE_OPTIONS ":c:"
#define PTHREAD_CALL(r, call) \
    if ((r = (call)) != 0) {  \
        errno = r;            \
        LOG_CRITICAL;         \
        kill(pid, SIGUSR1);   \
    }

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
    if ((error |= pthread_sigmask(SIG_SETMASK, &sigmask, NULL)) != 0) {
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
    LOG_DEBUG("casse iniziali = %d", config.casse_iniziali);
    LOG_DEBUG("t singolo prodotto = %d", config.t_singolo_prodotto);
    LOG_DEBUG("t aggiornamento clienti = %d", config.t_agg_clienti);
    LOG_DEBUG("T = %d", config.t_max);
    LOG_DEBUG("P = %d", config.p_max);
    LOG_DEBUG("K = %d", config.k_tot);
    LOG_DEBUG("S1 = %d", config.s1);
    LOG_DEBUG("S2 = %d", config.s2);
    LOG_DEBUG("C = %d", config.c_max);
    LOG_DEBUG("E = %d", config.e);
    LOG_DEBUG("LOG FILENAME = %s", config.log_file_name);

    /* strutture dati da utilizzare */
    sig_handler_opt_t sig_hand_opt;                   /* struttura del signal handler */
    cliente_opt_t clienti_opt[config.c_max];          /* C strutture dei clienti */
    cassa_opt_t casse_opt[config.k_tot];              /* K strutture delle casse */
    direttore_opt_t direttore_opt;                    /* struttura del direttore */
    BQueue_t* code_casse[config.k_tot];               /* K code */
    cassa_state_t stato_casse[config.k_tot];          /* array degli stati delle casse */
    supermercato_state_t stato_supermercato = ATTIVO; /* stato del supermercato/direttore */

    int id_cliente = 0; /* id cliente progressivo */

    /* thread in gioco nel sistema */
    pthread_t th_clienti[config.c_max]; /* C threads clienti */
    pthread_t th_casse[config.k_tot];   /* K threads cassieri */
    pthread_t th_direttore;             /* thread direttore */
    pthread_t th_signal_handler;        /* thread signal handler */

    /* attributi dei threads */
    pthread_attr_t sh_attr; /* attributo del signal handler */

    /* mutex e variabili di condizione in gioco nel sistema */
    pthread_mutex_t main_mutex;                 /* mutex principale */
    pthread_mutex_t quit_mutex;                 /* mutex riservata a supermercato, direttore e signal handler */
    pthread_mutex_t exit_mutex;                 /* mutex riservata a supermercato e clienti */
    pthread_mutex_t client_mutex[config.c_max]; /* mutex personale dei clienti, protegge dati accessibili anche dalle casse */

    pthread_cond_t auth_cond;                 /* var. condizione autorizzazione clienti */
    pthread_cond_t cond_casse[config.k_tot];  /* var. condizione casse */
    pthread_cond_t cond_incoda[config.c_max]; /* var. condizione clienti, quando vanno in coda */

    /*  dichiarazione altre variabili */
    int num_casse_attive = config.casse_iniziali; /* casse attualmente attive */
    bool auth_array[config.c_max];                /* array autorizzazioni (clienti-direttore) */
    bool exit_array[config.c_max];                /* array segnalazione uscita (clienti-supermercato) */
    int exited_clients = 0;                       /* numero di clienti usciti */
    int queue_notify[config.k_tot];               /* array per notifiche grandezza coda */
    volatile sig_atomic_t casse_partite = false;  /* informa il direttore quando le casse sono tutte "partite" */
    unsigned seed_direttore = time(NULL);

    /* inizializzazione mutex */
    PTHREAD_CALL(error, pthread_mutex_init(&main_mutex, NULL));
    PTHREAD_CALL(error, pthread_mutex_init(&quit_mutex, NULL));
    PTHREAD_CALL(error, pthread_mutex_init(&exit_mutex, NULL));
    for (size_t i = 0; i < config.c_max; i++) {
        PTHREAD_CALL(error, pthread_mutex_init(&client_mutex[i], NULL));
    }

    /* inizializzazione var. condizione */
    PTHREAD_CALL(error, pthread_cond_init(&auth_cond, NULL));
    for (size_t i = 0; i < config.k_tot; i++) {
        PTHREAD_CALL(error, pthread_cond_init(&cond_casse[i], NULL));
    }
    for (size_t i = 0; i < config.c_max; i++) {
        PTHREAD_CALL(error, pthread_cond_init(&cond_incoda[i], NULL));
    }

    /* inizializzazione attributi */
    PTHREAD_CALL(error, pthread_attr_init(&sh_attr));
    PTHREAD_CALL(error, pthread_attr_setdetachstate(&sh_attr, PTHREAD_CREATE_DETACHED));

    /* allocazione code */
    for (size_t i = 0; i < config.k_tot; i++) {
        if (CHECK_NULL(code_casse[i] = init_BQueue(config.c_max))) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
    }

    /*inizializzazione altre variabili */
    for (size_t i = 0; i < config.c_max; i++) {
        auth_array[i] = exit_array[i] = false;
    }
    for (size_t i = 0; i < config.k_tot; i++) {
        queue_notify[i] = 0;
        stato_casse[i] = (i < config.casse_iniziali ? APERTA : CHIUSA);
    }

    /* creazione thread signal handler */

    sig_hand_opt.quit_mutex = &quit_mutex;
    sig_hand_opt.stato_supermercato = &stato_supermercato;

    PTHREAD_CALL(error, pthread_create(&th_signal_handler, &sh_attr, signal_handler, &sig_hand_opt));

    /* creazione thread direttore */
    direttore_opt.quit_mutex = &quit_mutex;
    direttore_opt.stato_direttore = &stato_supermercato;
    direttore_opt.main_mutex = &main_mutex;
    direttore_opt.casse = casse_opt;
    direttore_opt.auth_cond = &auth_cond;
    direttore_opt.auth_array = auth_array;
    direttore_opt.queue_notify = queue_notify;
    direttore_opt.num_casse_attive = &num_casse_attive;
    direttore_opt.num_clienti = config.c_max;
    direttore_opt.casse_tot = config.k_tot;
    direttore_opt.soglia_1 = config.s1;
    direttore_opt.soglia_2 = config.s2;
    direttore_opt.casse_partite = &casse_partite;
    direttore_opt.seed = seed_direttore;

    PTHREAD_CALL(error, pthread_create(&th_direttore, NULL, direttore, &direttore_opt));

    /* creazione threads casse */
    for (size_t i = 0; i < config.k_tot; i++) {
        casse_opt[i].main_mutex = &main_mutex;
        casse_opt[i].stato_cassa = &stato_casse[i];
        casse_opt[i].cond = &cond_casse[i];
        casse_opt[i].coda = code_casse[i];
        casse_opt[i].queue_size_notify = &queue_notify[i];
        casse_opt[i].id_cassa = i;
        unsigned seed = (i + 1) * time(NULL);
        int t_fisso = rand_r(&seed) % (MAX_TF_CASSA - MIN_TF_CASSA + 1) + MIN_TF_CASSA;
        casse_opt[i].tempo_fisso = t_fisso;
        casse_opt[i].tempo_prodotto = config.t_singolo_prodotto;
        casse_opt[i].intervallo_notifica = config.t_agg_clienti;

        PTHREAD_CALL(error, pthread_create(&th_casse[i], NULL, cassa, &casse_opt[i]));
    }
    casse_partite = true;

    /* creazione threads clienti */
    for (size_t i = 0; i < config.c_max; i++) {
        clienti_opt[i].mutex_cliente = &client_mutex[i];
        clienti_opt[i].stato_cliente = ENTRATO;
        clienti_opt[i].cond_incoda = &cond_incoda[i];
        clienti_opt[i].main_mutex = &main_mutex;
        clienti_opt[i].is_authorized = &auth_array[i];
        clienti_opt[i].auth_cond = &auth_cond;
        clienti_opt[i].stato_casse = stato_casse;
        clienti_opt[i].coda_casse = code_casse;
        clienti_opt[i].num_casse_attive = &num_casse_attive;
        clienti_opt[i].exit_mutex = &exit_mutex;
        clienti_opt[i].is_exited = &exit_array[i];
        clienti_opt[i].num_exited = &exited_clients;
        clienti_opt[i].id_cliente = id_cliente++;
        unsigned seed = (i + 1) * time(NULL);
        int n_prod = rand_r(&seed) % (config.p_max + 1);
        int t_acquisti = rand_r(&seed) % (config.t_max - MIN_T_ACQUISTI + 1) + MIN_T_ACQUISTI;
        clienti_opt[i].num_prodotti = n_prod;
        clienti_opt[i].tempo_acquisti = t_acquisti;
        clienti_opt[i].casse_tot = config.k_tot;
        clienti_opt[i].seed = seed;

        PTHREAD_CALL(error, pthread_create(&th_clienti[i], NULL, cliente, &clienti_opt[i]));
    }

    while (1) {
        if (mutex_lock(&quit_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        if (stato_supermercato != ATTIVO) {
            if (mutex_unlock(&quit_mutex) != 0) {
                LOG_CRITICAL;
                kill(pid, SIGUSR1);
            }
            break;
        }
        if (mutex_unlock(&quit_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        if (mutex_lock(&exit_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        if (exited_clients >= config.e) {
            // ricicla threads
            for (size_t i = 0; i < config.c_max; i++) {
                if (*(clienti_opt[i].is_exited)) {
                    PTHREAD_CALL(error, pthread_join(th_clienti[i], NULL));
                    /* setto solo variabili che identificano un thread */
                    clienti_opt[i].stato_cliente = ENTRATO;
                    *(clienti_opt[i].is_authorized) = false;
                    *(clienti_opt[i].is_exited) = false;
                    clienti_opt[i].num_exited = &exited_clients;
                    clienti_opt[i].id_cliente = id_cliente++;
                    unsigned seed = (i + 1) * time(NULL);
                    int n_prod = rand_r(&seed) % (config.p_max + 1);
                    int t_acquisti = rand_r(&seed) % (config.t_max - MIN_T_ACQUISTI + 1) + MIN_T_ACQUISTI;
                    clienti_opt[i].num_prodotti = n_prod;
                    clienti_opt[i].tempo_acquisti = t_acquisti;

                    PTHREAD_CALL(error, pthread_create(&th_clienti[i], NULL, cliente, &clienti_opt[i]));
                }
            }
            exited_clients -= config.e;
        }
        if (mutex_unlock(&exit_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
    }

    for (size_t i = 0; i < config.c_max; i++) {
        PTHREAD_CALL(error, pthread_join(th_clienti[i], NULL));
    }
    for (size_t i = 0; i < config.k_tot; i++) {
        PTHREAD_CALL(error, pthread_join(th_casse[i], NULL));
    }
    PTHREAD_CALL(error, pthread_join(th_direttore, NULL));

    /* deallocazione mutex, var. condizione e attributi */
    pthread_mutex_destroy(&main_mutex);
    pthread_mutex_destroy(&quit_mutex);
    pthread_mutex_destroy(&exit_mutex);
    for (size_t i = 0; i < config.c_max; i++) {
        if (&client_mutex[i]) pthread_mutex_destroy(&client_mutex[i]);
    }

    pthread_cond_destroy(&auth_cond);
    for (size_t i = 0; i < config.k_tot; i++) {
        if (&cond_casse[i]) pthread_cond_destroy(&cond_casse[i]);
    }
    for (size_t i = 0; i < config.c_max; i++) {
        if (&cond_incoda[i]) pthread_cond_destroy(&cond_incoda[i]);
    }

    pthread_attr_destroy(&sh_attr);

    /* deallocazione altre strutture */
    for (size_t i = 0; i < config.k_tot; i++) {
        if (code_casse[i]) delete_BQueue(code_casse[i], NULL);
    }

    return 0;
}