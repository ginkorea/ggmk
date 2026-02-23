/*
 * GGMK/cpu — Arena init/destroy (delegates to HAL)
 */
#include "ggmk/alloc.h"
#include "ggmk/hal.h"

int gmk_arena_init(gmk_arena_t *a, size_t size) {
    if (!a || size == 0) return -1;

    a->base = (uint8_t *)gmk_hal_page_alloc(size, GMK_CACHE_LINE);
    if (!a->base) return -1;

    a->size = size;
    return 0;
}

void gmk_arena_destroy(gmk_arena_t *a) {
    if (a && a->base) {
        gmk_hal_page_free(a->base, a->size);
        a->base = NULL;
        a->size = 0;
    }
}
