/*
 * GGMK/cpu — SPSC ring buffer implementation
 */
#include "ggmk/ring_spsc.h"
#include "ggmk/hal.h"

int gmk_ring_spsc_init(gmk_ring_spsc_t *r, uint32_t cap, uint32_t elem_size) {
    if (!r || !gmk_is_power_of_two(cap) || elem_size == 0)
        return -1;

    r->cap       = cap;
    r->mask      = cap - 1;
    r->elem_size = elem_size;
    r->buf       = (uint8_t *)gmk_hal_page_alloc((size_t)cap * elem_size,
                                                   GMK_CACHE_LINE);
    if (!r->buf) return -1;

    atomic_init(&r->head, 0);
    atomic_init(&r->tail, 0);
    return 0;
}

int gmk_ring_spsc_init_buf(gmk_ring_spsc_t *r, uint32_t cap,
                            uint32_t elem_size, void *buf) {
    if (!r || !gmk_is_power_of_two(cap) || elem_size == 0 || !buf)
        return -1;

    r->cap       = cap;
    r->mask      = cap - 1;
    r->elem_size = elem_size;
    r->buf       = (uint8_t *)buf;

    atomic_init(&r->head, 0);
    atomic_init(&r->tail, 0);
    return 0;
}

void gmk_ring_spsc_destroy(gmk_ring_spsc_t *r) {
    if (r && r->buf) {
        gmk_hal_page_free(r->buf, (size_t)r->cap * r->elem_size);
        r->buf = NULL;
    }
}

int gmk_ring_spsc_push(gmk_ring_spsc_t *r, const void *elem) {
    uint32_t tail = gmk_atomic_load(&r->tail, memory_order_relaxed);
    uint32_t head = gmk_atomic_load(&r->head, memory_order_acquire);

    if (tail - head >= r->cap)
        return -1; /* full */

    uint32_t idx = tail & r->mask;
    gmk_hal_memcpy(r->buf + (size_t)idx * r->elem_size, elem, r->elem_size);

    gmk_atomic_store(&r->tail, tail + 1, memory_order_release);
    return 0;
}

int gmk_ring_spsc_pop(gmk_ring_spsc_t *r, void *elem) {
    uint32_t head = gmk_atomic_load(&r->head, memory_order_relaxed);
    uint32_t tail = gmk_atomic_load(&r->tail, memory_order_acquire);

    if (head == tail)
        return -1; /* empty */

    uint32_t idx = head & r->mask;
    gmk_hal_memcpy(elem, r->buf + (size_t)idx * r->elem_size, r->elem_size);

    gmk_atomic_store(&r->head, head + 1, memory_order_release);
    return 0;
}

uint32_t gmk_ring_spsc_count(const gmk_ring_spsc_t *r) {
    uint32_t tail = gmk_atomic_load(&r->tail, memory_order_acquire);
    uint32_t head = gmk_atomic_load(&r->head, memory_order_acquire);
    return tail - head;
}

bool gmk_ring_spsc_full(const gmk_ring_spsc_t *r) {
    return gmk_ring_spsc_count(r) >= r->cap;
}

bool gmk_ring_spsc_empty(const gmk_ring_spsc_t *r) {
    return gmk_ring_spsc_count(r) == 0;
}
