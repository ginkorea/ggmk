/*
 * GGMK/cpu — Worker thread pool
 *
 * Hosted: N pthreads, park via HAL condvar.
 * Freestanding: N CPUs, park via HAL sti;hlt, wake via HAL LAPIC IPI.
 */
#ifndef GMK_WORKER_H
#define GMK_WORKER_H

#include "types.h"
#include "sched.h"
#include "module.h"
#include "hal.h"

typedef struct {
    uint32_t        id;
    gmk_hal_thread_t thread;
    gmk_hal_park_t   park;
    gmk_sched_t    *sched;
    gmk_module_reg_t *modules;
    gmk_alloc_t    *alloc;
    gmk_chan_reg_t  *chan;
    gmk_trace_t    *trace;
    gmk_metrics_t  *metrics;
    gmk_kernel_t   *kernel;

    _Atomic(bool)   running;
    _Atomic(bool)   parked;

    _Atomic(uint64_t) tasks_dispatched;
    _Atomic(uint32_t) tick;
} gmk_worker_t;

typedef struct {
    gmk_worker_t   *workers;
    uint32_t         n_workers;
    gmk_sched_t    *sched;
    gmk_module_reg_t *modules;
    gmk_alloc_t    *alloc;
    gmk_chan_reg_t  *chan;
    gmk_trace_t    *trace;
    gmk_metrics_t  *metrics;
    gmk_kernel_t   *kernel;
} gmk_worker_pool_t;

int  gmk_worker_pool_init(gmk_worker_pool_t *pool, uint32_t n_workers,
                          gmk_sched_t *sched, gmk_module_reg_t *modules,
                          gmk_alloc_t *alloc, gmk_chan_reg_t *chan,
                          gmk_trace_t *trace, gmk_metrics_t *metrics,
                          gmk_kernel_t *kernel);
int  gmk_worker_pool_start(gmk_worker_pool_t *pool);
void gmk_worker_pool_stop(gmk_worker_pool_t *pool);
void gmk_worker_pool_destroy(gmk_worker_pool_t *pool);
void gmk_worker_wake(gmk_worker_t *w);
void gmk_worker_wake_all(gmk_worker_pool_t *pool);

/* Worker loop entry point (bare-metal APs call directly, hosted via HAL thread) */
void *gmk_worker_loop(void *arg);

#endif /* GMK_WORKER_H */
