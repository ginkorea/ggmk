/*
 * GMK/cpu — Linux HAL type definitions
 */
#ifndef GMK_HAL_LINUX_TYPES_H
#define GMK_HAL_LINUX_TYPES_H

#include <pthread.h>

typedef struct gmk_hal_thread {
    pthread_t pt;
} gmk_hal_thread_t;

typedef struct gmk_hal_lock {
    pthread_mutex_t m;
} gmk_hal_lock_t;

typedef struct gmk_hal_park {
    pthread_mutex_t m;
    pthread_cond_t  c;
} gmk_hal_park_t;

#endif /* GMK_HAL_LINUX_TYPES_H */
