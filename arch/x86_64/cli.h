/*
 * GMK/cpu — Kernel monitor CLI over serial
 */
#ifndef GMK_CLI_H
#define GMK_CLI_H

#include "../../include/gmk/boot.h"

/* Run the CLI loop on BSP. Returns when user types 'halt'. */
void cli_run(gmk_kernel_t *kernel);

#endif /* GMK_CLI_H */
