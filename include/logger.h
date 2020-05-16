#ifndef LOGGER_H
#define LOGGER_H

enum __loglevel {
    LOGLEVEL_CRITICAL,  // 0
    LOGLEVEL_DEBUG,     // 1
};

static char* levelname(enum __loglevel level) {
    static char* levels[] = {"CRITICAL", "DEBUG"};
    return levels[level];
}

#ifndef LOG_LEVEL
#define LOG_LEVEL LOGLEVEL_DEBUG
#endif /* LOG_LEVEL */

#ifndef LOG_OUTFILE
#define LOG_OUTFILE stdout
#endif /* LOG_OUTFILE */

#define LOG(level, ...)                                                                                                \
    {                                                                                                                  \
        if (level <= LOG_LEVEL) {                                                                                      \
            fprintf(LOG_OUTFILE, "[ %s ] ON %s AT LINE %d : %s\n", levelname(level), __FILE__, __LINE__, __VA_ARGS__); \
            fflush(LOG_OUTFILE);                                                                                       \
        }                                                                                                              \
    }
#define LOG_CRITICAL(...) LOG(LOGLEVEL_CRITICAL, __VA_ARGS__)
#define LOG_DEBUG(...) LOG(LOGLEVEL_DEBUG, __VA_ARGS__)

#endif /* LOGGER_H */