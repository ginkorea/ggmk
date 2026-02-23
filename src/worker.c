/*
 * GGMK/cpu — Worker thread loop (gather-dispatch-park)
 */
#include "ggmk/worker.h"
#include "ggmk/alloc.h"
#include "ggmk/trace.h"
#include "ggmk/metrics.h"
#include "ggmk/hal.h"

static void worker_dispatch_task(gmk_worker_t *w, gmk_task_t *task) {
    gmk_ctx_t ctx = {
        .task      = task,
        .alloc     = w->alloc,
        .chan       = w->chan,
        .trace     = w->trace,
        .metrics   = w->metrics,
        .sched     = w->sched,
        .kernel    = w->kernel,
        .worker_id = w->id,
        .tick      = gmk_atomic_load(&w->tick, memory_order_relaxed),
    };

    if (w->metrics)
        gmk_metric_inc(w->metrics, task->tenant,
                      GMK_METRIC_TASKS_DISPATCHED, 1);

    int rc = gmk_module_dispatch(w->modules, &ctx);

    if (rc == GMK_OK) {
        gmk_atomic_add(&w->tasks_dispatched, 1, memory_order_relaxed);
        /* Release refcounted payload — handler is done with it */
        if ((task->flags & GMK_TF_PAYLOAD_RC) && task->payload_ptr)
            gmk_payload_release(w->alloc, (void *)(uintptr_t)task->payload_ptr);
    } else if (rc == GMK_RETRY) {
        /* Re-enqueue for retry — keep payload ref alive */
        _gmk_enqueue(w->sched, task, -1);
        if (w->metrics)
            gmk_metric_inc(w->metrics, task->tenant,
                          GMK_METRIC_TASKS_RETRIED, 1);
    } else {
        /* Failure — release refcounted payload */
        gmk_module_record_fail(w->modules, task->type);
        if ((task->flags & GMK_TF_PAYLOAD_RC) && task->payload_ptr)
            gmk_payload_release(w->alloc, (void *)(uintptr_t)task->payload_ptr);
        if (w->metrics)
            gmk_metric_inc(w->metrics, task->tenant,
                          GMK_METRIC_TASKS_FAILED, 1);
    }
}

void *gmk_worker_loop(void *arg) {
    gmk_worker_t *w = (gmk_worker_t *)arg;
    gmk_task_t task;

    while (gmk_atomic_load(&w->running, memory_order_acquire)) {
        bool got_work = false;

        /* 1. Pop from own LQ */
        if (gmk_lq_pop(&w->sched->lqs[w->id], &task) == 0) {
            got_work = true;
            if (w->metrics)
                gmk_metric_inc(w->metrics, task.tenant,
                              GMK_METRIC_TASKS_DEQUEUED, 1);
            worker_dispatch_task(w, &task);
        }

        /* 2. Pop from overflow bucket */
        if (!got_work && gmk_ring_mpmc_pop(&w->sched->overflow, &task) == 0) {
            got_work = true;
            if (w->metrics)
                gmk_metric_inc(w->metrics, task.tenant,
                              GMK_METRIC_TASKS_DEQUEUED, 1);
            worker_dispatch_task(w, &task);
        }

        /* 3. Pop from RQ */
        if (!got_work && gmk_rq_pop(&w->sched->rq, &task) == 0) {
            got_work = true;
            if (w->metrics)
                gmk_metric_inc(w->metrics, task.tenant,
                              GMK_METRIC_TASKS_DEQUEUED, 1);
            worker_dispatch_task(w, &task);
        }

        /* 4. Check EVQ for due events */
        if (!got_work) {
            uint32_t tick = gmk_atomic_load(&w->tick, memory_order_relaxed);
            uint32_t evq_drained = 0;
            while (evq_drained < GMK_EVQ_DRAIN_LIMIT &&
                   gmk_evq_pop_due(&w->sched->evq, tick, &task) == 0) {
                got_work = true;
                evq_drained++;
                _gmk_enqueue(w->sched, &task, (int)w->id);
            }
        }

        /* 5. Park if no work */
        if (!got_work) {
            gmk_atomic_store(&w->parked, true, memory_order_release);
            if (w->metrics)
                gmk_metric_inc(w->metrics, 0, GMK_METRIC_WORKER_PARKS, 1);
            if (w->trace)
                gmk_trace_write(w->trace, 0, GMK_EV_WORKER_PARK,
                               0, w->id, 0);

            if (gmk_atomic_load(&w->running, memory_order_acquire))
                gmk_hal_park_wait(&w->park, 1000000); /* 1ms timeout */

            gmk_atomic_store(&w->parked, false, memory_order_release);
            if (w->metrics)
                gmk_metric_inc(w->metrics, 0, GMK_METRIC_WORKER_WAKES, 1);
        }
    }

    return NULL;
}

int gmk_worker_pool_init(gmk_worker_pool_t *pool, uint32_t n_workers,
                         gmk_sched_t *sched, gmk_module_reg_t *modules,
                         gmk_alloc_t *alloc, gmk_chan_reg_t *chan,
                         gmk_trace_t *trace, gmk_metrics_t *metrics,
                         gmk_kernel_t *kernel) {
    if (!pool || n_workers == 0 || !sched || !modules)
        return -1;

    pool->workers = (gmk_worker_t *)gmk_hal_calloc(n_workers, sizeof(gmk_worker_t));
    if (!pool->workers) return -1;

    pool->n_workers = n_workers;
    pool->sched     = sched;
    pool->modules   = modules;
    pool->alloc     = alloc;
    pool->chan       = chan;
    pool->trace     = trace;
    pool->metrics   = metrics;
    pool->kernel    = kernel;

    for (uint32_t i = 0; i < n_workers; i++) {
        gmk_worker_t *w = &pool->workers[i];
        w->id      = i;
        w->sched   = sched;
        w->modules = modules;
        w->alloc   = alloc;
        w->chan     = chan;
        w->trace   = trace;
        w->metrics = metrics;
        w->kernel  = kernel;
        atomic_init(&w->running, false);
        atomic_init(&w->parked, false);
        atomic_init(&w->tasks_dispatched, 0);
        atomic_init(&w->tick, 0);
        gmk_hal_park_init(&w->park);
    }

    return 0;
}

int gmk_worker_pool_start(gmk_worker_pool_t *pool) {
    if (!pool) return -1;

    for (uint32_t i = 0; i < pool->n_workers; i++) {
        gmk_worker_t *w = &pool->workers[i];
        gmk_atomic_store(&w->running, true, memory_order_release);
        if (gmk_hal_thread_create(&w->thread, gmk_worker_loop, w) != 0) {
            /* Stop already-started workers */
            for (uint32_t j = 0; j < i; j++) {
                gmk_atomic_store(&pool->workers[j].running, false,
                                memory_order_release);
                gmk_worker_wake(&pool->workers[j]);
                gmk_hal_thread_join(&pool->workers[j].thread);
            }
            return -1;
        }
    }
    return 0;
}

void gmk_worker_pool_stop(gmk_worker_pool_t *pool) {
    if (!pool) return;

    /* Signal all workers to stop */
    for (uint32_t i = 0; i < pool->n_workers; i++)
        gmk_atomic_store(&pool->workers[i].running, false, memory_order_release);

    /* Wake all parked workers */
    gmk_worker_wake_all(pool);

    /* Join all threads */
    for (uint32_t i = 0; i < pool->n_workers; i++)
        gmk_hal_thread_join(&pool->workers[i].thread);
}

void gmk_worker_pool_destroy(gmk_worker_pool_t *pool) {
    if (!pool) return;
    if (pool->workers) {
        for (uint32_t i = 0; i < pool->n_workers; i++)
            gmk_hal_park_destroy(&pool->workers[i].park);
        gmk_hal_free(pool->workers);
        pool->workers = NULL;
    }
}

void gmk_worker_wake(gmk_worker_t *w) {
    if (!w) return;
    if (gmk_atomic_load(&w->parked, memory_order_acquire))
        gmk_hal_park_wake(&w->park);
}

void gmk_worker_wake_all(gmk_worker_pool_t *pool) {
    if (!pool) return;
    for (uint32_t i = 0; i < pool->n_workers; i++)
        gmk_worker_wake(&pool->workers[i]);
}
