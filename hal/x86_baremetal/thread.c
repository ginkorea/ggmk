/*
 * GGMK/cpu — x86 bare-metal HAL: thread (no-op, APs pre-started by SMP)
 */
#include "ggmk/hal.h"

int gmk_hal_thread_create(gmk_hal_thread_t *t, void *(*fn)(void *), void *arg) {
    (void)t; (void)fn; (void)arg;
    return 0;
}

int gmk_hal_thread_join(gmk_hal_thread_t *t) {
    (void)t;
    return 0;
}
