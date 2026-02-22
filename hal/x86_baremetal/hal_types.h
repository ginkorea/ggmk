/*
 * GMK/cpu — x86 bare-metal HAL type definitions
 */
#ifndef GMK_HAL_X86_BM_TYPES_H
#define GMK_HAL_X86_BM_TYPES_H

#include "../../include/gmk/arch/spinlock.h"

typedef struct gmk_hal_thread {
    uint32_t cpu_id;
} gmk_hal_thread_t;

typedef struct gmk_hal_lock {
    gmk_spinlock_t s;
} gmk_hal_lock_t;

typedef struct gmk_hal_park {
    uint32_t cpu_id;
} gmk_hal_park_t;

#endif /* GMK_HAL_X86_BM_TYPES_H */
