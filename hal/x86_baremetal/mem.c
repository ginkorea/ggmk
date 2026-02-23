/*
 * GGMK/cpu — x86 bare-metal HAL: memory
 *
 * page_alloc/page_free: PMM (physical page allocator) — for arenas, ring buffers.
 * calloc/free: boot bump allocator — for small kernel-lifetime objects.
 */
#include "ggmk/hal.h"
#include "../../arch/x86_64/boot_alloc.h"
#include "../../arch/x86_64/pmm.h"
#include "../../arch/x86_64/mem.h"
#include <string.h>

void *gmk_hal_page_alloc(size_t size, size_t align) {
    (void)align; /* PMM returns 4KB-aligned pages, satisfies any power-of-two align <= 4096 */
    size_t pages = (size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    uint64_t phys = pmm_alloc_pages(pages);
    if (!phys) return NULL;
    void *ptr = phys_to_virt(phys);
    memset(ptr, 0, size);
    return ptr;
}

void gmk_hal_page_free(void *ptr, size_t size) {
    if (!ptr) return;
    size_t pages = (size + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    pmm_free_pages(virt_to_phys(ptr), pages);
}

void *gmk_hal_calloc(size_t n, size_t size) {
    return boot_calloc(n, size);
}

void gmk_hal_free(void *ptr) {
    boot_free(ptr);
}

void *gmk_hal_memset(void *dst, int val, size_t n) {
    return memset(dst, val, n);
}

void *gmk_hal_memcpy(void *dst, const void *src, size_t n) {
    return memcpy(dst, src, n);
}
