/*
 * GGMK/cpu — x86 bare-metal HAL: park (sti;hlt + LAPIC IPI)
 */
#include "ggmk/hal.h"
#include "../../arch/x86_64/lapic.h"

void gmk_hal_park_init(gmk_hal_park_t *p) {
    p->cpu_id = 0;
}

void gmk_hal_park_wait(gmk_hal_park_t *p, uint64_t timeout_ns) {
    (void)p; (void)timeout_ns;
    __asm__ volatile("sti; hlt; cli");
}

void gmk_hal_park_wake(gmk_hal_park_t *p) {
    lapic_send_ipi(p->cpu_id, IPI_WAKE_VECTOR);
}

void gmk_hal_park_destroy(gmk_hal_park_t *p) {
    (void)p;
}
