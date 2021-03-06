#ifndef FILESTAT_H
#define FILESTAT_H

#include <bqueue.h>
#include <queue.h>

#define VAR_PATH "./var"
#define VAR_RUN_PATH "./var/run"
#define PID_FILENAME "./var/run/sm.pid"
#define LOG_FILENAME "./var/run/log_filename.txt"

/**
 * scrive il pid del processo in un file
 * 
 * @return 0 se successo, -1 se fallisce
*/
int write_pid(pid_t pid);

/**
 * scrive il nome del file di log in un file
 * 
 * @return 0 se successo, -1 se fallisce
*/
int write_log_filename(const char* log_filename);

/** 
 * scrive statistiche su file di log
 * 
 * @return 0 se successo, -1 se fallisce
*/
int print_to_log(int num_clienti, Queue_t* clienti, cassa_opt_t* casse, int k, const char* filename);

#endif /* FILESTAT_H */