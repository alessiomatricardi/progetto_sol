#define _POSIX_C_SOURCE 200112L
#include <ctype.h>
#include <parsing.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * nome dei campi nel file di configurazione
*/
#define CASSE_INIT "CASSE_INIT"
#define T_SINGOLO_PROD "T_SING_PROD"
#define T_AGG_NUM_CLIENTI "T_AGG_NUM_C"
#define MAX_T_ACQUISTI "T"
#define MAX_NUM_PRODOTTI "P"
#define MAX_CASSE "K"
#define SOGLIA1 "S1"
#define SOGLIA2 "S2"
#define MAX_NUM_CLIENTI "C"
#define E_CLIENTI "E"
#define LOG_FILENAME "LOG_FILENAME"

/* grandezza della tabella hash */
#define BUCKETS 50

/*
 * dealloca tabella hash e ritorna -1 in caso di fallimento di fun
 * se presente, esegue anche altra funzione (variadic argument)
*/
#define CHECK_AND_FREE(fun, fail_ret, ht)     \
    do {                                      \
        if ((fun) == (fail_ret)) {            \
            icl_hash_destroy(ht, free, free); \
            return -1;                        \
        }                                     \
    } while (0);

static int is_empty(const char* s) {
    while (*s != '\0') {
        if (!isspace(*s))
            return 0;
        s++;
    }
    return 1;
}

int config_to_hashtable(FILE* f, icl_hash_t* hash_table) {
    char buffer[MAX_BUFFER];
    char temp_nome[MAX_CONFIG_ATTR_SIZE];
    char temp_valore[MAX_CONFIG_ATTR_SIZE];
    char* nome = NULL;
    char* valore = NULL;
    char* save_ptr = NULL;
    char* token_nome = NULL;
    char* token_valore = NULL;
    while (fgets(buffer, MAX_BUFFER, f)) {
        if (buffer[0] == '#' || is_empty(buffer)) continue;
        nome = NULL;
        valore = NULL;
        memset(temp_nome, '\0', MAX_CONFIG_ATTR_SIZE);
        memset(temp_valore, '\0', MAX_CONFIG_ATTR_SIZE);
        if (sscanf(buffer, "%127[^=]=%127[^\n]", temp_nome, temp_valore) != 2) {
            perror("sscanf");
            return -1;
        }
        token_nome = strtok_r(temp_nome, " ", &save_ptr);
        token_valore = strtok_r(temp_valore, " ", &save_ptr);
        if (!token_nome) {
            perror("strtok token_nome");
            return -1;
        }
        if (!token_valore) {
            perror("strtok token_valore");
            return -1;
        }
        int len_nome = strlen(token_nome)+1;
        int len_valore = strlen(token_valore)+1;
        nome = calloc(len_nome, sizeof(char));      // deallocato dalla tabella hash
        valore = calloc(len_valore, sizeof(char));  // deallocato dalla tabella hash
        if (!nome) {
            perror("calloc nome");
            return -1;
        }
        if (!valore) {
            perror("calloc valore");
            return -1;
        }
        memcpy(nome, token_nome, (len_nome-1) * sizeof(char));
        memcpy(valore, token_valore, (len_valore-1) * sizeof(char));
        if (!icl_hash_insert(hash_table, nome, (void*)valore)) return -1;
    }
    return 0;
}

int set_value(void* value, icl_hash_t* hash_table, char* key, bool is_string) {
    char* temp = (char*)icl_hash_find(hash_table, key);
    if (temp == NULL) return -1;
    if (is_string) {
        memcpy((char*)value, temp, strlen(temp) * sizeof(char));
        return 0;
    }
    int x;
    if (sscanf(temp, "%d", &x) != 1) {
        perror("set value");
        return -1;
    }
    if (x <= 0) {
        fprintf(stderr, "Immesso valore negativo o 0\n");
        return -1;
    }
    *((int*)value) = x;
    return 0;
}

int parse_config(const char* filename, config_t* config) {
    FILE* f;
    if ((f = fopen(filename, "r")) == NULL) {
        perror("file di configurazione - apertura");
        return -1;
    }

    icl_hash_t* hash_table = NULL;
    hash_table = icl_hash_create(BUCKETS, NULL, NULL);
    if (!hash_table) {
        fclose(f);
        return -1;
    }

    if (config_to_hashtable(f, hash_table) == -1) {
        icl_hash_destroy(hash_table, free, free);
        fclose(f);
        return -1;
    }
    CHECK_AND_FREE(fclose(f), EOF, hash_table);

    CHECK_AND_FREE(set_value(&config->casse_iniziali, hash_table, CASSE_INIT, false), -1, hash_table);
    CHECK_AND_FREE(set_value(&config->t_singolo_prodotto, hash_table, T_SINGOLO_PROD, false), -1, hash_table);
    CHECK_AND_FREE(set_value(&config->t_agg_clienti, hash_table, T_AGG_NUM_CLIENTI, false), -1, hash_table);
    CHECK_AND_FREE(set_value(&config->t_max, hash_table, MAX_T_ACQUISTI, false), -1, hash_table);
    CHECK_AND_FREE(set_value(&config->p_max, hash_table, MAX_NUM_PRODOTTI, false), -1, hash_table);
    CHECK_AND_FREE(set_value(&config->k_tot, hash_table, MAX_CASSE, false), -1, hash_table);
    CHECK_AND_FREE(set_value(&config->s1, hash_table, SOGLIA1, false), -1, hash_table);
    CHECK_AND_FREE(set_value(&config->s2, hash_table, SOGLIA2, false), -1, hash_table);
    CHECK_AND_FREE(set_value(&config->c_max, hash_table, MAX_NUM_CLIENTI, false), -1, hash_table);
    CHECK_AND_FREE(set_value(&config->e, hash_table, E_CLIENTI, false), -1, hash_table);
    CHECK_AND_FREE(set_value(config->log_file_name, hash_table, LOG_FILENAME, true), -1, hash_table);

    if (icl_hash_destroy(hash_table, free, free) == -1) return -1;

    /* controlli di qualità */
    /* tutti i valori numerici sono già > 0 */
    if (config->t_max <= 10) {
        fprintf(stderr, "T deve essere maggiore di 10\n");
        return -1;
    }
    if (config->e >= config->c_max) {
        fprintf(stderr, "E deve essere minore di C\n");
        return -1;
    }
    if (config->casse_iniziali > config->k_tot) {
        fprintf(stderr, "Il numero di casse inizialmente aperte deve essere minore del numero totale\n");
        return -1;
    }
    return 0;
}