/*
 * GGMK/cpu — x86 bare-metal HAL: lock (spinlock wrappers)
 */
#include "ggmk/hal.h"

void gmk_hal_lock_init(gmk_hal_lock_t *l) {
    gmk_spinlock_init(&l->s);
}

void gmk_hal_lock_acquire(gmk_hal_lock_t *l) {
    gmk_spinlock_acquire(&l->s);
}

void gmk_hal_lock_release(gmk_hal_lock_t *l) {
    gmk_spinlock_release(&l->s);
}

void gmk_hal_lock_destroy(gmk_hal_lock_t *l) {
    gmk_spinlock_destroy(&l->s);
}
