/*
 * GGMK/cpu — Hardware Abstraction Layer
 *
 * Platform-neutral API. The ONLY file in core that selects a platform.
 * Core compiles anywhere. HAL is "the current planet."
 */
#ifndef GMK_HAL_H
#define GMK_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Platform type selection (the ONE #ifdef) ────────────────── */
#ifdef GMK_FREESTANDING
#include "../../hal/x86_baremetal/hal_types.h"
#else
#include "../../hal/linux/hal_types.h"
#endif

/* ── Thread ──────────────────────────────────────────────────── */
int  gmk_hal_thread_create(gmk_hal_thread_t *t, void *(*fn)(void *), void *arg);
int  gmk_hal_thread_join(gmk_hal_thread_t *t);

/* ── Lock ────────────────────────────────────────────────────── */
void gmk_hal_lock_init(gmk_hal_lock_t *l);
void gmk_hal_lock_acquire(gmk_hal_lock_t *l);
void gmk_hal_lock_release(gmk_hal_lock_t *l);
void gmk_hal_lock_destroy(gmk_hal_lock_t *l);

/* ── Park (thread sleep/wake) ────────────────────────────────── */
void gmk_hal_park_init(gmk_hal_park_t *p);
void gmk_hal_park_wait(gmk_hal_park_t *p, uint64_t timeout_ns);
void gmk_hal_park_wake(gmk_hal_park_t *p);
void gmk_hal_park_destroy(gmk_hal_park_t *p);

/* ── Time ────────────────────────────────────────────────────── */
uint64_t gmk_hal_now_ns(void);

/* ── Memory ──────────────────────────────────────────────────── */
void *gmk_hal_page_alloc(size_t size, size_t align);
void  gmk_hal_page_free(void *ptr, size_t size);
void *gmk_hal_calloc(size_t n, size_t size);
void  gmk_hal_free(void *ptr);
void *gmk_hal_memset(void *dst, int val, size_t n);
void *gmk_hal_memcpy(void *dst, const void *src, size_t n);

#endif /* GMK_HAL_H */
