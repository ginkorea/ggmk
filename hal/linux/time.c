/*
 * GGMK/cpu — Linux HAL: time (clock_gettime wrapper)
 */
#include "ggmk/hal.h"
#include <time.h>

uint64_t gmk_hal_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
