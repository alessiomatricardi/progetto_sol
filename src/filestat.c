#include <errno.h>
#include <filestat.h>
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

int print_to_log(int num_clienti, int num_prodotti,
    Queue_t* clienti, BQueue_t* cassieri, const char* filename) {
    
}