/*
 * GMK/cpu — Linux HAL: memory (libc wrappers)
 */
#include "gmk/hal.h"
#include <stdlib.h>
#include <string.h>

void *gmk_hal_page_alloc(size_t size, size_t align) {
    if (align < sizeof(void *))
        align = sizeof(void *);
    /* aligned_alloc requires size to be a multiple of alignment */
    size = (size + align - 1) & ~(align - 1);
    void *p = aligned_alloc(align, size);
    if (p) memset(p, 0, size);
    return p;
}

void gmk_hal_page_free(void *ptr, size_t size) {
    (void)size;
    free(ptr);
}

void *gmk_hal_calloc(size_t n, size_t size) {
    return calloc(n, size);
}

void gmk_hal_free(void *ptr) {
    free(ptr);
}

void *gmk_hal_memset(void *dst, int val, size_t n) {
    return memset(dst, val, n);
}

void *gmk_hal_memcpy(void *dst, const void *src, size_t n) {
    return memcpy(dst, src, n);
}
