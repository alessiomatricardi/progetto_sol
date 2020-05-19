#ifndef LOGGER_H
#define LOGGER_H

#include <errno.h>
#include <stdio.h>
#include <string.h>

enum __loglevel {
    LOGLEVEL_CRITICAL,  // 0
    LOGLEVEL_DEBUG,     // 1
};

static char* levelname(enum __loglevel level) {
    static char* levels[] = {"CRITICAL", "DEBUG"};
    return levels[level];
}

#ifndef DEBUG
#define DEBUG 1
#endif /* DEBUG */

#ifndef LOG_OUTFILE
#define LOG_OUTFILE stdout
#endif /* LOG_OUTFILE */

#define LOG(level, format, ...)                                         \
    {                                                                   \
        do {                                                            \
            fprintf(LOG_OUTFILE, "[ %s ] %s:%d " format "\n",           \
                    levelname(level), __FILE__, __LINE__, __VA_ARGS__); \
            fflush(LOG_OUTFILE);                                        \
        } while (0);                                                    \
    }

#define LOG_0(level, msg)                                                            \
    {                                                                                \
        do {                                                                         \
            fprintf(LOG_OUTFILE,                                                     \
                    "[ %s ] %s:%d %s\n", levelname(level), __FILE__, __LINE__, msg); \
            fflush(LOG_OUTFILE);                                                     \
        } while (0);                                                                 \
    }

/* usa LOG_DEBUG0 se non vuoi passare parametri */
#define LOG_DEBUG(format, ...)                               \
    do {                                                     \
        if (DEBUG) LOG(LOGLEVEL_DEBUG, format, __VA_ARGS__); \
    } while (0);

/* usa LOG_DEBUG se vuoi passare parametri */
#define LOG_DEBUG0(msg)                        \
    do {                                       \
        if (DEBUG) LOG_0(LOGLEVEL_DEBUG, msg); \
    } while (0);

#define LOG_CRITICAL                               \
    do {                                           \
        LOG_0(LOGLEVEL_CRITICAL, strerror(errno)); \
    } while (0);

#endif /* LOGGER_H */