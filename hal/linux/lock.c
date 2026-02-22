/*
 * GMK/cpu — Linux HAL: lock (pthread_mutex wrappers)
 */
#include "gmk/hal.h"
#include <pthread.h>

void gmk_hal_lock_init(gmk_hal_lock_t *l) {
    pthread_mutex_init(&l->m, NULL);
}

void gmk_hal_lock_acquire(gmk_hal_lock_t *l) {
    pthread_mutex_lock(&l->m);
}

void gmk_hal_lock_release(gmk_hal_lock_t *l) {
    pthread_mutex_unlock(&l->m);
}

void gmk_hal_lock_destroy(gmk_hal_lock_t *l) {
    pthread_mutex_destroy(&l->m);
}
