/*
 * GGMK/cpu — Kernel main: boot GGMK, submit test tasks, enter CLI
 */
#include "serial.h"
#include "lapic.h"
#include "smp.h"
#include "idt.h"
#include "vmm.h"
#include "pci.h"
#include "boot_alloc.h"
#include "pmm.h"
#include "mem.h"
#include "cli.h"

#include "../../include/ggmk/boot.h"
#include "../../include/ggmk/worker.h"
#include "../../drivers/virtio/virtio_blk.h"

/* ── Echo handler: prints task info to serial ───────────────────── */
static int echo_handler(gmk_ctx_t *ctx) {
    kprintf("[worker %u] echo: type=%u meta0=%lu\n",
            ctx->worker_id, ctx->task->type,
            (unsigned long)ctx->task->meta0);
    return GMK_OK;
}

/* ── Echo module definition ─────────────────────────────────────── */
static gmk_handler_reg_t echo_handlers[] = {
    {
        .type       = 1,
        .fn         = echo_handler,
        .name       = "echo",
        .flags      = GMK_HF_SCALAR,
        .max_yields = 0,
    },
};

static gmk_module_t echo_module = {
    .name       = "echo",
    .version    = GMK_VERSION(0, 1, 0),
    .handlers   = echo_handlers,
    .n_handlers = 1,
    .channels   = 0,
    .n_channels = 0,
    .init       = 0,
    .fini       = 0,
};

/* ── CLI module definition (PRD §4.12) ─────────────────────────── */
static gmk_chan_decl_t cli_channels[] = {
    { .name = "cli.cmd",  .direction = GMK_CHAN_PRODUCE, .msg_type = 0,
      .mode = GMK_CHAN_P2P, .guarantee = GMK_CHAN_LOSSLESS },
    { .name = "cli.resp", .direction = GMK_CHAN_CONSUME, .msg_type = 0,
      .mode = GMK_CHAN_P2P, .guarantee = GMK_CHAN_LOSSLESS },
};

static gmk_module_t cli_module = {
    .name       = "cli",
    .version    = GMK_VERSION(0, 1, 0),
    .handlers   = 0,
    .n_handlers = 0,
    .channels   = cli_channels,
    .n_channels = 2,
    .init       = 0,
    .fini       = 0,
};

/* ── Kernel state (global so APs can access it) ─────────────────── */
static gmk_kernel_t kernel;

/* Per-AP ready flag: set by each AP before entering worker loop */
static volatile _Atomic(uint32_t) ap_ready_count;

/* ── AP entry: find our worker and run the worker loop ──────────── */
static void ap_worker_entry(void *arg) {
    (void)arg;

    /* Find our worker by matching LAPIC ID */
    uint32_t my_lapic = lapic_id();
    gmk_worker_t *w = 0;
    for (uint32_t i = 0; i < kernel.pool.n_workers; i++) {
        if (kernel.pool.workers[i].park.cpu_id == my_lapic) {
            w = &kernel.pool.workers[i];
            break;
        }
    }

    if (w) {
        kprintf("CPU %u entering worker loop (LAPIC %u)\n", w->id, my_lapic);
        __atomic_fetch_add(&ap_ready_count, 1, __ATOMIC_RELEASE);
        gmk_worker_loop(w);
    }

    /* Should not return; halt if it does */
    for (;;) __asm__ volatile("cli; hlt");
}

/* ── Kernel main ────────────────────────────────────────────────── */
void kmain(uint32_t cpu_count) {
    kprintf("\n=== GGMK/cpu kernel main ===\n");

    /* Configure boot */
    gmk_boot_cfg_t cfg = {
        .arena_size = GMK_DEFAULT_ARENA_SIZE,
        .n_workers  = cpu_count > GMK_MAX_WORKERS ? GMK_MAX_WORKERS : cpu_count,
        .n_tenants  = GMK_DEFAULT_TENANTS,
    };

    /* Register modules */
    gmk_module_t *modules[] = { &echo_module, &cli_module };

    kprintf("Booting GGMK kernel: %u workers, %lu MB arena\n",
            cfg.n_workers, (unsigned long)(cfg.arena_size >> 20));

    /* Boot the kernel subsystems */
    int rc = gmk_boot(&kernel, &cfg, modules, 2);
    if (rc != 0) {
        PANIC("gmk_boot failed (rc=%d)", rc);
    }

    kprintf("GGMK kernel booted successfully\n");

    /* VMM smoke test: kmalloc + demand page + read/write + kfree */
    {
        volatile uint8_t *p = (volatile uint8_t *)kmalloc(4096);
        kprintf("VMM test: kmalloc(4096) = %p\n", (void *)p);
        p[0] = 0xAB;    /* triggers demand page fault → auto-map */
        p[4095] = 0xCD;
        kprintf("VMM test: p[0]=0x%x p[4095]=0x%x\n",
                (unsigned)p[0], (unsigned)p[4095]);
        kfree((void *)p, 4096);
        kprintf("VMM test: kfree OK\n");
    }

    /* Virtio-blk smoke test */
    {
        pci_device_t *vblk = pci_find_device(0x1AF4, 0x1001);
        if (vblk) {
            kprintf("virtio-blk device found at PCI %u:%u.%u\n",
                    vblk->bus, vblk->dev, vblk->func);
            int vrc = virtio_blk_init(vblk);
            if (vrc == 0) {
                /* Write a pattern to sector 0 */
                uint8_t wbuf[512];
                for (int i = 0; i < 512; i++)
                    wbuf[i] = (uint8_t)(i & 0xFF);
                vrc = virtio_blk_write(0, wbuf);
                kprintf("virtio-blk: write sector 0: %s\n",
                        vrc == 0 ? "OK" : "FAIL");

                /* Read it back */
                uint8_t rbuf[512];
                for (int i = 0; i < 512; i++) rbuf[i] = 0;
                vrc = virtio_blk_read(0, rbuf);
                kprintf("virtio-blk: read sector 0: %s\n",
                        vrc == 0 ? "OK" : "FAIL");

                /* Verify */
                int match = 1;
                for (int i = 0; i < 512; i++) {
                    if (rbuf[i] != (uint8_t)(i & 0xFF)) { match = 0; break; }
                }
                kprintf("virtio-blk: verify: %s\n",
                        match ? "PASS" : "FAIL");
            }
        } else {
            kprintf("virtio-blk: no device found (add -device virtio-blk-pci to QEMU)\n");
        }
    }

    /* Assign LAPIC IDs to workers for IPI wake */
    kernel.pool.workers[0].park.cpu_id = smp_bsp_lapic_id();
    for (uint32_t i = 1; i < cfg.n_workers; i++) {
        kernel.pool.workers[i].park.cpu_id = smp_lapic_id(i);
    }

    /* Start APs first so they're in worker loops before tasks arrive */
    if (cpu_count > 1) {
        __atomic_store_n(&ap_ready_count, 0, __ATOMIC_RELEASE);
        smp_start_aps(ap_worker_entry, &kernel.pool);

        /* Wait for all APs to reach worker loop (with timeout) */
        uint32_t expected = cpu_count - 1;
        for (volatile uint32_t timeout = 0; timeout < 10000000; timeout++) {
            if (__atomic_load_n(&ap_ready_count, __ATOMIC_ACQUIRE) >= expected)
                break;
            __asm__ volatile("pause");
        }
        uint32_t ready = __atomic_load_n(&ap_ready_count, __ATOMIC_ACQUIRE);
        kprintf("APs ready: %u / %u\n", ready, expected);
        vmm_set_cpu_count(cpu_count);
    }

    /* Submit 16 echo tasks (APs are now running) */
    kprintf("Submitting 16 tasks...\n");
    for (uint32_t i = 0; i < 16; i++) {
        gmk_task_t task = {0};
        task.type  = 1; /* echo handler */
        task.flags = GMK_SET_PRIORITY(0, GMK_PRIO_NORMAL);
        task.meta0 = i;
        task.meta1 = 0;

        rc = gmk_submit(&kernel, &task);
        if (rc != 0) {
            kprintf("  submit %u failed: %d\n", i, rc);
        }
    }
    kprintf("Submitted 16 tasks.\n");

    /* BSP enters CLI loop (worker 0 is never started — BSP is console) */
    cli_run(&kernel);

    /* CLI returned (user typed 'halt') — system idle */
    kprintf("\nGMK kernel halted. System idle.\n");
    for (;;) __asm__ volatile("sti; hlt");
}
