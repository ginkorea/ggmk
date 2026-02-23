/*
 * GGMK/cpu — Lock abstraction (delegates to HAL)
 */
#ifndef GMK_LOCK_H
#define GMK_LOCK_H

#include "hal.h"

typedef gmk_hal_lock_t gmk_lock_t;

#define gmk_lock_init(l)    gmk_hal_lock_init(l)
#define gmk_lock_destroy(l) gmk_hal_lock_destroy(l)
#define gmk_lock_acquire(l) gmk_hal_lock_acquire(l)
#define gmk_lock_release(l) gmk_hal_lock_release(l)

#endif /* GMK_LOCK_H */
