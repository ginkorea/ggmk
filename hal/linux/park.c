/*
 * GGMK/cpu — Linux HAL: park (condvar with CLOCK_MONOTONIC)
 */
#include "ggmk/hal.h"
#include <pthread.h>
#include <time.h>

void gmk_hal_park_init(gmk_hal_park_t *p) {
    pthread_mutex_init(&p->m, NULL);

    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&p->c, &attr);
    pthread_condattr_destroy(&attr);
}

void gmk_hal_park_wait(gmk_hal_park_t *p, uint64_t timeout_ns) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_nsec += (long)timeout_ns;
    while (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&p->m);
    pthread_cond_timedwait(&p->c, &p->m, &ts);
    pthread_mutex_unlock(&p->m);
}

void gmk_hal_park_wake(gmk_hal_park_t *p) {
    pthread_mutex_lock(&p->m);
    pthread_cond_signal(&p->c);
    pthread_mutex_unlock(&p->m);
}

void gmk_hal_park_destroy(gmk_hal_park_t *p) {
    pthread_cond_destroy(&p->c);
    pthread_mutex_destroy(&p->m);
}
