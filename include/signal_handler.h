#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H
#include <direttore.h>

typedef struct _signal_handler {
    supermercato_state_t* stato_supermercato;
    pthread_mutex_t* quit_mutex;
} sig_handler_opt_t;

void* signal_handler(void* arg);

#endif /* SIGNAL_HANDLER_H */