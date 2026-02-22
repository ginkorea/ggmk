/*
 * GMK/cpu — Linux HAL: thread (pthread wrappers)
 */
#include "gmk/hal.h"
#include <pthread.h>

int gmk_hal_thread_create(gmk_hal_thread_t *t, void *(*fn)(void *), void *arg) {
    if (!t) return -1;
    return pthread_create(&t->pt, NULL, fn, arg) == 0 ? 0 : -1;
}

int gmk_hal_thread_join(gmk_hal_thread_t *t) {
    if (!t) return -1;
    return pthread_join(t->pt, NULL) == 0 ? 0 : -1;
}
