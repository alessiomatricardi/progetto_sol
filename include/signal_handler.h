#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H
#include <direttore.h>

typedef struct _signal_handler {
    supermercato_state_t stato_supermercato;
    /* altro */
} sig_handler_opt_t;

void* signal_handler(void* arg);

#endif /* SIGNAL_HANDLER_H */