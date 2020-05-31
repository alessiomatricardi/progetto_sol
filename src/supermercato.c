#define _POSIX_C_SOURCE 200112L
#include <bool.h>
#include <cassa.h>
#include <cliente.h>
#include <direttore.h>
#include <filestat.h>
#include <logger.h>
#include <parsing.h>
#include <queue.h>
#include <signal.h>
#include <signal_handler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

/* variabili globali */

/* thread in gioco nel sistema */
static pthread_t* th_clienti;       /* C threads clienti */
static pthread_t* th_casse;         /* K threads cassieri */
static pthread_t th_direttore;      /* thread direttore */
static pthread_t th_signal_handler; /* thread signal handler */

/* attributi dei threads */
static pthread_attr_t sh_attr;     /* attributo del signal handler */
static pthread_attr_t* casse_attr; /* attributo delle casse */

/* mutex e variabili di condizione in gioco nel sistema */
static pthread_mutex_t main_mutex;    /* mutex principale */
static pthread_mutex_t quit_mutex;    /* mutex riservata a supermercato, direttore e signal handler */
static pthread_mutex_t exit_mutex;    /* mutex riservata a supermercato e clienti */
static pthread_mutex_t* client_mutex; /* mutex personale dei clienti, protegge dati accessibili anche dalle casse */

static pthread_cond_t auth_cond;    /* var. condizione autorizzazione clienti */
static pthread_cond_t* cond_incoda; /* var. condizione clienti, quando vanno in coda */
static pthread_cond_t exit_cond;    /* var. condizione clienti-supermercato per segnalare uscita */
static pthread_cond_t notify_cond;  /* var. condizione casse-direttor per l'invio delle notifiche */

/* altre variabili globali */
static config_t config;       /* dove salvare dati in ingresso */
static int id_cliente = 0;    /* id cliente progressivo, alla fine rappresenterà anche il numero totale di clienti */
static int num_casse_attive;  /* casse attualmente attive */
static bool* auth_array;      /* array autorizzazioni (clienti-direttore) */
static bool* exit_array;      /* array segnalazione uscita (clienti-supermercato) */
static int exited_clients;    /* numero di clienti usciti */
static int* queue_notify;     /* array per notificare grandezza coda */
static bool* notify_sent;     /* array per dire al direttore di aver notificato */
volatile int should_quit = 0; /* i cassieri ed il direttore possono terminare */
volatile int need_auth = 1;   /* i clienti hanno bisogno dell'autorizzazione? */
static BQueue_t** code_casse; /* K code */
static Queue_t* stat_clienti; /* coda di clienti per salvataggio su log */

static bool check_apertura_supermercato(supermercato_state_t stato);
static void init_direttore(direttore_opt_t* direttore_opt, direttore_state_t* stato, cassa_opt_t* casse);
static void init_cassa(cassa_opt_t* cassa_opt, size_t pos, cassa_state_t* stato, BQueue_t* coda);
static void init_cliente(cliente_opt_t* cliente_opt, size_t pos, cassa_state_t* stati_casse, BQueue_t** code);
static void cleanup();
/* funzione di lettura dei parametri passati da terminale */
static int set_config_filename(char* config_filename, int argc, char** argv) {
    if (argc == 3) {
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
    } else {
        strncpy(config_filename, DEFAULT_CONFIG_FILENAME, strlen(DEFAULT_CONFIG_FILENAME) + 1);
        return 0;
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
    if (write_pid(pid) == -1) {
        fprintf(stderr, "Non è stato possibile salvare l'id del processo.\n");
        return EXIT_FAILURE;
    }

    if (argc != 3 && argc != 1) {
        printf("Usage: %s -c <config file>\n", argv[0]);
        printf("or use default: %s [-c config.txt]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    /* set file name del config */
    char config_filename[MAX_CONFIG_FILENAME] = {'\0'};
    if (set_config_filename(config_filename, argc, argv) == -1) {
        return EXIT_FAILURE;
    }
    /* caricamento del file di config in struct dedicata */
    memset(&config, 0, sizeof(config_t));
    if (parse_config(config_filename, &config) != 0) {
        printf("Qualcosa è andato storto durante la lettura del file di configurazione.\n");
        exit(EXIT_FAILURE);
    }
    /* stampa debug */
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

    /* scrivo il nome del file di log prodotto alla terminazione */
    if (write_log_filename(config.log_file_name) == -1) {
        fprintf(stderr, "Non è stato possibile salvare il nome del file di log\n");
        return EXIT_FAILURE;
    }

    /* strutture dati da utilizzare */
    sig_handler_opt_t sig_hand_opt;                   /* struttura del signal handler */
    cliente_opt_t* clienti_opt[config.c_max];         /* strutture dei clienti */
    cassa_opt_t casse_opt[config.k_tot];              /* K strutture delle casse */
    direttore_opt_t direttore_opt;                    /* struttura del direttore */
    cassa_state_t stato_casse[config.k_tot];          /* array degli stati delle casse */
    supermercato_state_t stato_supermercato = ATTIVO; /* stato del supermercato/direttore */

    /* allocazione threads, mutex e var. condizione */
    th_clienti = malloc(config.c_max * sizeof(pthread_t));
    if (CHECK_NULL(th_clienti)) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    th_casse = malloc(config.k_tot * sizeof(pthread_t));
    if (CHECK_NULL(th_casse)) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    client_mutex = malloc(config.c_max * sizeof(pthread_mutex_t));
    if (CHECK_NULL(client_mutex)) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    cond_incoda = malloc(config.c_max * sizeof(pthread_cond_t));
    if (CHECK_NULL(cond_incoda)) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    casse_attr = malloc(config.k_tot * sizeof(pthread_attr_t));
    if (CHECK_NULL(casse_attr)) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }

    /* inizializzazione mutex */
    PTHREAD_CALL(error, pthread_mutex_init(&main_mutex, NULL));
    PTHREAD_CALL(error, pthread_mutex_init(&quit_mutex, NULL));
    PTHREAD_CALL(error, pthread_mutex_init(&exit_mutex, NULL));
    for (size_t i = 0; i < config.c_max; i++) {
        PTHREAD_CALL(error, pthread_mutex_init(&client_mutex[i], NULL));
    }

    /* inizializzazione var. condizione */
    PTHREAD_CALL(error, pthread_cond_init(&auth_cond, NULL));
    for (size_t i = 0; i < config.c_max; i++) {
        PTHREAD_CALL(error, pthread_cond_init(&cond_incoda[i], NULL));
    }
    PTHREAD_CALL(error, pthread_cond_init(&exit_cond, NULL));
    PTHREAD_CALL(error, pthread_cond_init(&notify_cond, NULL));

    /* inizializzazione attributi */
    PTHREAD_CALL(error, pthread_attr_init(&sh_attr));
    PTHREAD_CALL(error, pthread_attr_setdetachstate(&sh_attr, PTHREAD_CREATE_DETACHED));
    for (size_t i = 0; i < config.k_tot; i++) {
        PTHREAD_CALL(error, pthread_attr_init(&casse_attr[i]));
    }

    /* allocazione array di strutture clienti iniziali */
    for (size_t i = 0; i < config.c_max; i++) {
        clienti_opt[i] = malloc(sizeof(cliente_opt_t));
        if (CHECK_NULL(clienti_opt[i])) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
    }

    /* allocazione code casse */
    if (CHECK_NULL(code_casse = malloc(config.k_tot * sizeof(BQueue_t*)))) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    for (size_t i = 0; i < config.k_tot; i++) {
        if (CHECK_NULL(code_casse[i] = init_BQueue(config.c_max))) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
    }

    /* allocazione coda stat_clienti */
    if (CHECK_NULL(stat_clienti = initQueue())) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }

    /* allocazione e/o inizializzazione altre variabili */
    num_casse_attive = config.casse_iniziali;
    auth_array = malloc(config.c_max * sizeof(bool));  /* array autorizzazioni (clienti-direttore) */
    exit_array = malloc(config.c_max * sizeof(bool));  /* array segnalazione uscita (clienti-supermercato) */
    exited_clients = 0;                                /* numero di clienti usciti */
    queue_notify = malloc(config.k_tot * sizeof(int)); /* array notifiche */
    notify_sent = malloc(config.k_tot * sizeof(bool)); /* array 'ho notificato' */
    for (size_t i = 0; i < config.c_max; i++) {
        auth_array[i] = true; /* saranno poi i clienti con 0 prodotti a settarlo a false */
        exit_array[i] = false;
    }
    for (size_t i = 0; i < config.k_tot; i++) {
        queue_notify[i] = 0;
        notify_sent[i] = false;
        stato_casse[i] = (i < config.casse_iniziali ? APERTA : CHIUSA);
    }

    /* creazione thread signal handler */
    sig_hand_opt.quit_mutex = &quit_mutex;
    sig_hand_opt.stato_supermercato = &stato_supermercato;

    PTHREAD_CALL(error, pthread_create(&th_signal_handler, &sh_attr, signal_handler, &sig_hand_opt));

    /* inizializzazione strutture casse */
    for (size_t i = 0; i < config.k_tot; i++) {
        init_cassa(&casse_opt[i], i, &stato_casse[i], code_casse[i]);
    }

    /* creazione thread direttore */
    init_direttore(&direttore_opt, &stato_supermercato, casse_opt);

    PTHREAD_CALL(error, pthread_create(&th_direttore, NULL, fun_direttore, &direttore_opt));

    /* creazione threads clienti */
    for (size_t i = 0; i < config.c_max; i++) {
        init_cliente(clienti_opt[i], i, stato_casse, code_casse);
        if (lpush(stat_clienti, clienti_opt[i]) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }

        PTHREAD_CALL(error, pthread_create(&th_clienti[i], NULL, fun_cliente, clienti_opt[i]));
    }

    while (1) {
        if (!check_apertura_supermercato(stato_supermercato)) break;

        if (mutex_lock(&exit_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        while (exited_clients < config.e) {
            if (cond_wait(&exit_cond, &exit_mutex) != 0) {
                LOG_CRITICAL;
                kill(pid, SIGUSR1);
            }
        }
        // ricicla threads clienti
        for (size_t i = 0; i < config.c_max; i++) {
            if (*(clienti_opt[i]->is_exited)) {
                PTHREAD_CALL(error, pthread_join(th_clienti[i], NULL));
                if (CHECK_NULL(clienti_opt[i] = malloc(sizeof(cliente_opt_t)))) {
                    LOG_CRITICAL;
                    kill(pid, SIGUSR1);
                }
                init_cliente(clienti_opt[i], i, stato_casse, code_casse);
                /* modifica solo variabili che identificano un thread */
                *(clienti_opt[i]->is_exited) = false;
                *(clienti_opt[i]->is_authorized) = false;
                if (lpush(stat_clienti, clienti_opt[i]) != 0) {
                    LOG_CRITICAL;
                    kill(pid, SIGUSR1);
                }

                PTHREAD_CALL(error, pthread_create(&th_clienti[i], NULL, fun_cliente, clienti_opt[i]));
                if (--exited_clients == 0) break;
            }
        }
        if (mutex_unlock(&exit_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
    }
    /* join clienti e direttore */
    for (size_t i = 0; i < config.c_max; i++) {
        PTHREAD_CALL(error, pthread_join(th_clienti[i], NULL));
    }
    should_quit = 1;
    PTHREAD_CALL(error, pthread_join(th_direttore, NULL));

    /* salvataggio su file di log */
    if (print_to_log(id_cliente, stat_clienti, casse_opt, config.k_tot, config.log_file_name) != 0) {
        fprintf(stderr, "Non è stato possibile scrivere il risultato dell'esecuzione sul file di log\n");
        return EXIT_FAILURE;
    }

    /* free & destroy */
    cleanup();

    fprintf(stdout, "CHIUSO!\n");
    return EXIT_SUCCESS;
}

static bool check_apertura_supermercato(supermercato_state_t stato) {
    if (mutex_lock(&quit_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    if (stato != ATTIVO) {
        if (mutex_unlock(&quit_mutex) != 0) {
            LOG_CRITICAL;
            kill(pid, SIGUSR1);
        }
        return false;
    }
    if (mutex_unlock(&quit_mutex) != 0) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    return true;
}

static void init_direttore(direttore_opt_t* direttore_opt, direttore_state_t* stato, cassa_opt_t* casse) {
    direttore_opt->quit_mutex = &quit_mutex;
    direttore_opt->stato_direttore = stato;
    direttore_opt->main_mutex = &main_mutex;
    direttore_opt->th_casse = th_casse;
    direttore_opt->attr_casse = casse_attr;
    direttore_opt->casse = casse;
    direttore_opt->auth_cond = &auth_cond;
    direttore_opt->auth_array = auth_array;
    direttore_opt->num_casse_attive = &num_casse_attive;
    direttore_opt->notify_cond = &notify_cond;
    direttore_opt->queue_notify = queue_notify;
    direttore_opt->notify_sent = notify_sent;
    direttore_opt->num_clienti = config.c_max;
    direttore_opt->num_casse_tot = config.k_tot;
    direttore_opt->soglia_1 = config.s1;
    direttore_opt->soglia_2 = config.s2;
}

static void init_cassa(cassa_opt_t* cassa_opt, size_t pos, cassa_state_t* stato, BQueue_t* coda) {
    cassa_opt->main_mutex = &main_mutex;
    cassa_opt->stato_cassa = stato;
    cassa_opt->coda = coda;
    cassa_opt->notify_cond = &notify_cond;
    cassa_opt->notify_size = &queue_notify[pos];
    cassa_opt->notify_sent = &notify_sent[pos];
    cassa_opt->id_cassa = pos;
    unsigned seed = (pos + 1) * time(NULL);
    int t_fisso = rand_r(&seed) % (MAX_TF_CASSA - MIN_TF_CASSA + 1) + MIN_TF_CASSA;
    cassa_opt->tempo_fisso = t_fisso;
    cassa_opt->tempo_prodotto = config.t_singolo_prodotto;
    cassa_opt->intervallo_notifica = config.t_agg_clienti;
    cassa_opt->num_clienti_serviti = 0;
    cassa_opt->num_prodotti_elaborati = 0;
    if (CHECK_NULL(cassa_opt->tempi_apertura = initQueue())) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
    cassa_opt->num_chiusure = 0;
    if (CHECK_NULL(cassa_opt->t_clienti_serviti = initQueue())) {
        LOG_CRITICAL;
        kill(pid, SIGUSR1);
    }
}

static void init_cliente(cliente_opt_t* cliente_opt, size_t pos, cassa_state_t* stati_casse, BQueue_t** code) {
    cliente_opt->mutex_cliente = &client_mutex[pos];
    cliente_opt->stato_cliente = ENTRATO;
    cliente_opt->cond_incoda = &cond_incoda[pos];
    cliente_opt->main_mutex = &main_mutex;
    cliente_opt->is_authorized = &auth_array[pos];
    cliente_opt->auth_cond = &auth_cond;
    cliente_opt->stato_casse = stati_casse;
    cliente_opt->coda_casse = code;
    cliente_opt->num_casse_attive = &num_casse_attive;
    cliente_opt->exit_mutex = &exit_mutex;
    cliente_opt->is_exited = &exit_array[pos];
    cliente_opt->num_exited = &exited_clients;
    cliente_opt->exit_cond = &exit_cond;
    cliente_opt->id_cliente = id_cliente++;
    unsigned seed = id_cliente * time(NULL);
    int n_prod = rand_r(&seed) % (config.p_max + 1);
    int t_acquisti = rand_r(&seed) % (config.t_max - MIN_T_ACQUISTI + 1) + MIN_T_ACQUISTI;
    cliente_opt->num_prodotti = n_prod;
    cliente_opt->tempo_acquisti = t_acquisti;
    cliente_opt->num_casse_tot = config.k_tot;
    cliente_opt->seed = seed;
    cliente_opt->t_permanenza = 0;
    cliente_opt->tstart_attesa_coda = (struct timespec){.tv_sec = 0, .tv_nsec = 0};
    cliente_opt->tend_attesa_coda = (struct timespec){.tv_sec = 0, .tv_nsec = 0};
    cliente_opt->num_cambi_coda = 0;
}

static void cleanup() {
    /* deallocazione mutex, var. condizione e attributi */

    if (th_clienti) free(th_clienti);
    if (th_casse) free(th_casse);

    pthread_mutex_destroy(&main_mutex);
    pthread_mutex_destroy(&quit_mutex);
    pthread_mutex_destroy(&exit_mutex);
    for (size_t i = 0; i < config.c_max; i++) {
        if (&client_mutex[i]) pthread_mutex_destroy(&client_mutex[i]);
    }
    if (client_mutex) free(client_mutex);

    pthread_cond_destroy(&auth_cond);
    for (size_t i = 0; i < config.c_max; i++) {
        if (&cond_incoda[i]) pthread_cond_destroy(&cond_incoda[i]);
    }
    free(cond_incoda);
    pthread_cond_destroy(&exit_cond);
    pthread_cond_destroy(&notify_cond);

    pthread_attr_destroy(&sh_attr);

    /* deallocazione altre strutture */
    for (size_t i = 0; i < config.k_tot; i++) {
        pthread_attr_destroy(&casse_attr[i]);
        if (code_casse[i]) delete_BQueue(code_casse[i], NULL);
    }
    if (code_casse) free(code_casse);
    if (casse_attr) free(casse_attr);
    if (auth_array) free(auth_array);
    if (exit_array) free(exit_array);
    if (queue_notify) free(queue_notify);
    if (notify_sent) free(notify_sent);
}