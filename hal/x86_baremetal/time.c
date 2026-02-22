/*
 * GMK/cpu — x86 bare-metal HAL: time (LAPIC timer tick count)
 */
#include "gmk/hal.h"
#include "../../arch/x86_64/idt.h"

uint64_t gmk_hal_now_ns(void) {
    return idt_get_timer_count() * 1000000ULL;  /* 1ms ticks -> ns */
}
