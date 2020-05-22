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

#define LOG(where, level, format, ...)                                  \
    {                                                                   \
        do {                                                            \
            fprintf(where, "[ %s ] %s:%d\t " format "\n",                 \
                    levelname(level), __FILE__, __LINE__, __VA_ARGS__); \
            fflush(where);                                              \
        } while (0);                                                    \
    }

#define LOG_0(where, level, msg)                                                     \
    {                                                                                \
        do {                                                                         \
            fprintf(where,                                                           \
                    "[ %s ] %s:%d\t %s\n", levelname(level), __FILE__, __LINE__, msg); \
            fflush(where);                                                           \
        } while (0);                                                                 \
    }

/* usa LOG_DEBUG0 se non vuoi passare parametri */
#define LOG_DEBUG(format, ...)                                       \
    do {                                                             \
        if (DEBUG) LOG(stdout, LOGLEVEL_DEBUG, format, __VA_ARGS__); \
    } while (0);

/* usa LOG_DEBUG se vuoi passare parametri */
#define LOG_DEBUG0(msg)                                \
    do {                                               \
        if (DEBUG) LOG_0(stdout, LOGLEVEL_DEBUG, msg); \
    } while (0);

#define LOG_CRITICAL                                       \
    do {                                                   \
        LOG_0(stderr, LOGLEVEL_CRITICAL, strerror(errno)); \
    } while (0);

#endif /* LOGGER_H */