/*
 * GGMK/cpu — Page table manipulation
 *
 * Provides 4KB page mapping/unmapping using the existing 4-level page tables
 * set up by Limine. Limine's HHDM only covers physical RAM; MMIO regions
 * and kernel heap pages must be manually mapped here.
 */
#include "paging.h"
#include "mem.h"
#include "pmm.h"
#include "serial.h"

uint64_t paging_read_cr3(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

uint64_t paging_alloc_table(void) {
    uint64_t phys = pmm_alloc_pages(1);
    if (!phys) {
        PANIC("out of memory for page table");
    }
    uint64_t *virt = (uint64_t *)phys_to_virt(phys);
    for (int i = 0; i < 512; i++)
        virt[i] = 0;
    return phys;
}

void paging_map(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags) {
    uint64_t cr3 = paging_read_cr3() & ~0xFFFULL;

    uint16_t pml4_idx = (virt_addr >> 39) & 0x1FF;
    uint16_t pdp_idx  = (virt_addr >> 30) & 0x1FF;
    uint16_t pd_idx   = (virt_addr >> 21) & 0x1FF;
    uint16_t pt_idx   = (virt_addr >> 12) & 0x1FF;

    uint64_t *pml4 = (uint64_t *)phys_to_virt(cr3);

    /* Ensure PDP table exists */
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        uint64_t page = paging_alloc_table();
        pml4[pml4_idx] = page | PTE_PRESENT | PTE_WRITABLE;
    }

    uint64_t *pdp = (uint64_t *)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);

    /* If PDP entry is a 1GB huge page, the region is already mapped */
    if ((pdp[pdp_idx] & PTE_PRESENT) && (pdp[pdp_idx] & PTE_PS))
        return;

    /* Ensure PD table exists */
    if (!(pdp[pdp_idx] & PTE_PRESENT)) {
        uint64_t page = paging_alloc_table();
        pdp[pdp_idx] = page | PTE_PRESENT | PTE_WRITABLE;
    }

    uint64_t *pd = (uint64_t *)phys_to_virt(pdp[pdp_idx] & ~0xFFFULL);

    /* If PD entry is a 2MB huge page, the region is already mapped */
    if ((pd[pd_idx] & PTE_PRESENT) && (pd[pd_idx] & PTE_PS))
        return;

    /* Ensure PT exists */
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        uint64_t page = paging_alloc_table();
        pd[pd_idx] = page | PTE_PRESENT | PTE_WRITABLE;
    }

    uint64_t *pt = (uint64_t *)phys_to_virt(pd[pd_idx] & ~0xFFFULL);

    /* Set the page table entry */
    pt[pt_idx] = (phys_addr & ~0xFFFULL) | flags;

    /* Invalidate TLB for this virtual address */
    paging_invlpg(virt_addr);
}

uint64_t paging_unmap(uint64_t virt_addr) {
    uint64_t cr3 = paging_read_cr3() & ~0xFFFULL;

    uint16_t pml4_idx = (virt_addr >> 39) & 0x1FF;
    uint16_t pdp_idx  = (virt_addr >> 30) & 0x1FF;
    uint16_t pd_idx   = (virt_addr >> 21) & 0x1FF;
    uint16_t pt_idx   = (virt_addr >> 12) & 0x1FF;

    uint64_t *pml4 = (uint64_t *)phys_to_virt(cr3);
    if (!(pml4[pml4_idx] & PTE_PRESENT)) return 0;

    uint64_t *pdp = (uint64_t *)phys_to_virt(pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdp[pdp_idx] & PTE_PRESENT)) return 0;
    if (pdp[pdp_idx] & PTE_PS) return 0; /* can't unmap from huge page */

    uint64_t *pd = (uint64_t *)phys_to_virt(pdp[pdp_idx] & ~0xFFFULL);
    if (!(pd[pd_idx] & PTE_PRESENT)) return 0;
    if (pd[pd_idx] & PTE_PS) return 0; /* can't unmap from huge page */

    uint64_t *pt = (uint64_t *)phys_to_virt(pd[pd_idx] & ~0xFFFULL);
    if (!(pt[pt_idx] & PTE_PRESENT)) return 0;

    uint64_t phys = pt[pt_idx] & ~0xFFFULL;
    pt[pt_idx] = 0;

    paging_invlpg(virt_addr);
    return phys;
}

int paging_walk(uint64_t virt, uint64_t *pml4e, uint64_t *pdpe,
                uint64_t *pde, uint64_t *pte) {
    uint64_t cr3 = paging_read_cr3() & ~0xFFFULL;

    uint16_t pml4_idx = (virt >> 39) & 0x1FF;
    uint16_t pdp_idx  = (virt >> 30) & 0x1FF;
    uint16_t pd_idx   = (virt >> 21) & 0x1FF;
    uint16_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t *pml4 = (uint64_t *)phys_to_virt(cr3);
    *pml4e = pml4[pml4_idx];
    *pdpe = *pde = *pte = 0;

    if (!(*pml4e & PTE_PRESENT))
        return 1;

    uint64_t *pdp = (uint64_t *)phys_to_virt(*pml4e & ~0xFFFULL);
    *pdpe = pdp[pdp_idx];

    if (!(*pdpe & PTE_PRESENT))
        return 2;
    if (*pdpe & PTE_PS)
        return 2; /* 1GB huge page */

    uint64_t *pd = (uint64_t *)phys_to_virt(*pdpe & ~0xFFFULL);
    *pde = pd[pd_idx];

    if (!(*pde & PTE_PRESENT))
        return 3;
    if (*pde & PTE_PS)
        return 3; /* 2MB huge page */

    uint64_t *pt = (uint64_t *)phys_to_virt(*pde & ~0xFFFULL);
    *pte = pt[pt_idx];
    return 4;
}

void map_mmio(uint64_t phys, size_t size) {
    uint64_t flags = PTE_PRESENT | PTE_WRITABLE | PTE_PCD | PTE_PWT;
    uint64_t end = (phys + size + 0xFFF) & ~0xFFFULL;
    uint64_t start = phys & ~0xFFFULL;

    for (uint64_t addr = start; addr < end; addr += 4096) {
        uint64_t virt = (uint64_t)phys_to_virt(addr);
        paging_map(virt, addr, flags);
    }
}
