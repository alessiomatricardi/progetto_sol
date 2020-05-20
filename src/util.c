#define _POSIX_C_SOURCE 200112L
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <util.h>

int msleep(long msec) {
    struct timespec ts;
    int res;

    if (msec < 0) {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

int mutex_lock(pthread_mutex_t* mutex) {
    int err = 0;
    if ((err = pthread_mutex_lock(mutex)) != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

int mutex_unlock(pthread_mutex_t* mutex) {
    int err = 0;
    if ((err = pthread_mutex_unlock(mutex)) != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

int cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
    int err = 0;
    if ((err = pthread_cond_wait(cond, mutex)) != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

int cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex, struct timespec* ts) {
    int err = 0;
    if ((err = pthread_cond_timedwait(cond, mutex, ts)) != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

int cond_signal(pthread_cond_t* cond) {
    int err = 0;
    if ((err = pthread_cond_signal(cond)) != 0) {
        errno = err;
        return -1;
    }
    return 0;
}

int cond_broadcast(pthread_cond_t* cond) {
    int err = 0;
    if ((err = pthread_cond_broadcast(cond)) != 0) {
        errno = err;
        return -1;
    }
    return 0;
}
