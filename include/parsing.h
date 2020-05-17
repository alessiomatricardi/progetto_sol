/**
 * parsing del file di configurazione
*/
#ifndef PARSING_H
#define PARSING_H
#include <bool.h>
#include <icl_hash.h>

/* grandezza massima del buffer (ogni riga del file config) */
#define MAX_BUFFER 256

/* grandezza massima nome/valore dentro config */
#define MAX_CONFIG_ATTR_SIZE 128

/* massima grandezza del nome del file di log */
#define MAX_LOG_FILENAME MAX_CONFIG_ATTR_SIZE

/* struct config */
typedef struct _config {
    int casse_iniziali;                   /* casse inizialmente aperte */
    int t_singolo_prodotto;               /* tempo impiegato da un cassiere per un singolo prodotto */
    int t_agg_clienti;                    /* tempo che intercorre tra una notifica ed un'altra */
    int t_max;                            /* T = tempo massimo che ha il cliente per acquistare */
    int p_max;                            /* P = numero massimo di prodotti acquistabili */
    int k_tot;                            /* K = numero di casse totali */
    int s1;                               /* Soglia 1 */
    int s2;                               /* Soglia 2 */
    int c_max;                            /* C = numero massimo di clienti nel supermercato */
    int e;                                /* E = quando il numero di clienti scende a C-E, entrano E nuovi clienti */
    char log_file_name[MAX_LOG_FILENAME]; /* nome del file dove scrivere il log risultante */
} config_t;

/**
 * salvataggio su hash table di tutti i campi del file di configurazione
 * 
 * @param f puntatore al file da leggere
 * @param hash_table tabella hash in cui vengono salvati i parametri
 * 
 * @return 0 sul successo, -1 sul fallimento (lettura fallita)
*/
int config_to_hashtable(FILE* f, icl_hash_t* hash_table);

/**
 * scrive valore della hash table nella variabile
 * 
 * @param value variabile da settare
 * @param hash_table tabella hash da cui estrapolare il valore
 * @param key chiave da cercare in tabella hash
 * @param is_string valore booleano che indica se la variabile passata è una stringa
 * 
 * @return 0 sul successo, -1 sul fallimento (key non trovata, valore numerico <= 0)
*/
int set_value(void* value, icl_hash_t* hash_table, char* key, bool is_string);

/**
 * salvataggio da hash table su variabili
 * 
 * @param filename nome del file di configurazione
 * @param variabili da settare
 * 
 * @return 0 sul successo, -1 sul fallimento (file inesistente, parametri mancanti)
*/
int parse_config(const char* filename, config_t* config);

#endif /* PARSING_H */