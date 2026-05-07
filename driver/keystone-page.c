//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
// Copyright (c) 2023, Akihiro Saiki. All Rights Reserved.
//
// SPDX-License-Identifier: BSD-3-Clause
//------------------------------------------------------------------------------
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/version.h>

#include "keystone.h"
#include "riscv64.h"

/* Destroy all memory associated with an EPM */
int epm_destroy(struct epm *epm) {
    if (!epm->ptr || !epm->alloc_ptr || !epm->size || !epm->alloc_size)
        return 0;

    /* free the EPM hold by the enclave */
    if (epm->is_cma) {
        dma_free_coherent(
            keystone_dev.this_device, epm->alloc_size, (void *)epm->alloc_ptr, epm->alloc_pa);
    } else {
        free_pages(epm->alloc_ptr, epm->order);
    }

    return 0;
}

/* Create an EPM and initialize the free list */
int epm_init(struct epm *epm, unsigned int min_pages) {
    vaddr_t epm_vaddr = 0;
    unsigned long order = 0;
    unsigned long count = min_pages;
    phys_addr_t device_phys_addr = 0;

    /* For eyrie megapage */
    vaddr_t aligned_vaddr = 0;
    paddr_t aligned_paddr = 0;
    paddr_t align_diff = 0;

    /* try to allocate contiguous memory */
    epm->is_cma = 0;
    order = ilog2(min_pages - 1) + 1;
    count = 0x1 << order;

    /* prevent kernel from complaining about an invalid argument */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
    if (order < MAX_PAGE_ORDER)
#else
    if (order < MAX_ORDER)
#endif
        epm_vaddr = (vaddr_t)__get_free_pages(GFP_HIGHUSER, order);

#ifdef CONFIG_CMA
    /* If buddy allocator fails, we fall back to the CMA */
    if (!epm_vaddr) {
        epm->is_cma = 1;
        count = min_pages;

        epm_vaddr = (vaddr_t)dma_alloc_coherent(
            keystone_dev.this_device, count << PAGE_SHIFT, &device_phys_addr,
            GFP_KERNEL | __GFP_DMA32);

        if (!device_phys_addr)
            epm_vaddr = 0;
    }

#endif

    if (!epm_vaddr) {
        keystone_err("failed to allocate %lu page(s)\n", count);
        return -ENOMEM;
    }

    /*
     * At this time, top addr is PAGE_SIZE (4KiB) aligned,
     * but not guaranteed to be megapage size (2MiB in sv39) aligned.
     * When epm size is larger than megapage size (2MiB), eyrie will use
     * megapage. So eyrie will fail to boot while remapping dram page.
     *
     * To deal with this feature, realloc epm with alignment offset.
     * The offset region will become dummy region and not be used.
     *
     *  __________________  epm_paddr
     * |                  |
     * |   dummy region   |
     * |__________________| aligned_paddr
     * |                  |
     * |                  |
     * |    real epm      |
     * |                  |
     * |                  |
     * ~------------------~
     * |                  |
     * |__________________| aligned_paddr + count << PAGE_SHIFT
     *
     * Total allocation size = count << PAGE_SHIFT + size of dummy region
     *
     */
    aligned_vaddr = epm_vaddr;
    aligned_paddr = __pa(epm_vaddr);
    if (epm->is_cma && count << PAGE_SHIFT > RISCV_GET_LVL_PGSIZE(2)) {
        /* calc required alignment */
        aligned_paddr = ROUND_UP(aligned_paddr, RISCV_GET_LVL_PGSIZE_BITS(2));
        align_diff = aligned_paddr - __pa(epm_vaddr);

        /* temporary free allocated mem */
        dma_free_coherent(
            keystone_dev.this_device, count << PAGE_SHIFT, (void *)epm_vaddr, __pa(epm_vaddr));

        /* realloc with alignment diff */
        epm_vaddr = (vaddr_t)dma_alloc_coherent(
            keystone_dev.this_device, (count << PAGE_SHIFT) + align_diff, &device_phys_addr,
            GFP_KERNEL | __GFP_DMA32);

        if (!device_phys_addr)
            epm_vaddr = 0;

        if (!epm_vaddr || aligned_paddr - __pa(epm_vaddr) != align_diff) {
            keystone_err("failed to realloc %lu byte(s)\n", (count << PAGE_SHIFT) + align_diff);
            return -ENOMEM;
        }

        aligned_vaddr = epm_vaddr + align_diff;
    }

    /* zero out */
    memset((void *)aligned_vaddr, 0, PAGE_SIZE * count);

    /* store params */
    epm->root_page_table =
        (void *)aligned_vaddr;       // ptr to root page table (top of the allocated va)
    epm->pa = aligned_paddr;         // physical address of epm (after aligned)
    epm->alloc_pa = __pa(epm_vaddr); // physical address of total allocated memory region
    epm->order = order;              // order for buddy allocation
    epm->size = count << PAGE_SHIFT; // size of epm (after aligned)
    epm->alloc_size = (count << PAGE_SHIFT) + align_diff; // size of total allocated memory region
    epm->ptr = aligned_vaddr;                             // ptr to epm in kernel va
    epm->alloc_ptr = epm_vaddr; // ptr to total allocated memory region in kernel va

    keystone_info("allocated private memory, paddr: %lx, size: %lx\n", epm->pa, epm->size);

    return 0;
}

int utm_destroy(struct utm *utm) {
    if (!utm->ptr || !utm->size) {
        return 0;
    }

    if (utm->is_cma) {
        dma_free_coherent(keystone_dev.this_device, utm->size, utm->ptr, __pa(utm->ptr));
    } else {
        free_pages((vaddr_t)utm->ptr, utm->order);
    }

    return 0;
}

int utm_init(struct utm *utm, size_t untrusted_size) {
    unsigned long req_pages = 0;
    unsigned long order = 0;
    unsigned long count;
    phys_addr_t device_phys_addr = 0;
    req_pages += PAGE_UP(untrusted_size) / PAGE_SIZE;
    order = ilog2(req_pages - 1) + 1;
    count = 0x1 << order;

    utm->order = order;
    utm->is_cma = false;
    utm->ptr = 0;

    /* Currently, UTM does not utilize CMA.
     * It is always allocated from the buddy allocator */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
    if (order < MAX_PAGE_ORDER)
#else
    if (order < MAX_ORDER)
#endif
        utm->ptr = (void *)__get_free_pages(GFP_HIGHUSER, order);

#ifdef CONFIG_CMA
    /* Use CMA */
    if (!utm->ptr) {
        keystone_info("using cma for shared memory\n");
        utm->is_cma = true;
        count = req_pages;
        utm->ptr = (void *)dma_alloc_coherent(
            keystone_dev.this_device, count * PAGE_SIZE, &device_phys_addr,
            GFP_KERNEL | __GFP_DMA32);

        if (!device_phys_addr)
            utm->ptr = 0;
    }
#endif

    if (!utm->ptr) {
        keystone_err("failed to allocate shared memory (size = %i bytes)\n", (1 << order));
        return -ENOMEM;
    }

    utm->size = count * PAGE_SIZE;
    if (req_pages * PAGE_SIZE != untrusted_size) {
        /* Instead of failing, we just warn that the user has to fix the
         * parameter. */
        keystone_warn("shared memory size is not multiple of PAGE_SIZE\n");
    }

    keystone_info("allocated shared memory, paddr: %lx, size: %lx\n", __pa(utm->ptr), utm->size);

    return 0;
}

int adm_destroy(struct adm *adm) {
    if (!adm->ptr || !adm->size) {
        return 0;
    }

    if (adm->is_cma) {
        dma_free_coherent(keystone_dev.this_device, adm->size, (void *)adm->ptr, adm->pa);
    } else {
        free_pages(adm->ptr, adm->order);
    }

    return 0;
}

int adm_init(struct adm *adm, size_t adm_size, __u8 protection) {
    unsigned long req_pages = 0;
    unsigned long order = 0;
    unsigned long count;
    phys_addr_t device_phys_addr = 0;
    req_pages += PAGE_UP(adm_size) / PAGE_SIZE;
    order = ilog2(req_pages - 1) + 1;
    count = 0x1 << order;

    adm->order = order;
    adm->is_cma = false;
    adm->ptr = 0;
    adm->protection = protection;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
    if (order < MAX_PAGE_ORDER)
#else
    if (order < MAX_ORDER)
#endif
        adm->ptr = (vaddr_t)__get_free_pages(GFP_HIGHUSER, order);

#ifdef CONFIG_CMA
    if (!adm->ptr) {
        /* Use CMA */
        keystone_info("using cma for data memory\n");
        adm->is_cma = true;
        count = req_pages;
        adm->ptr = (vaddr_t)dma_alloc_coherent(
            keystone_dev.this_device, count << PAGE_SHIFT, &device_phys_addr,
            GFP_KERNEL | __GFP_DMA32);

        if (!device_phys_addr)
            adm->ptr = 0;
    }
#endif

    if (!adm->ptr) {
        keystone_err("failed to allocate data memory (size = %i bytes)\n", (1 << order));
        return -ENOMEM;
    }

    adm->size = count * PAGE_SIZE;
    if (req_pages * PAGE_SIZE != adm_size) {
        /* Instead of failing, we just warn that the user has to fix the
         * parameter. */
        keystone_warn("data memory size is not multiple of PAGE_SIZE\n");
    }

    // Zero out
    memset((void *)adm->ptr, 0, adm->size);

    adm->pa = __pa(adm->ptr);

    keystone_info("allocated data memory, paddr: %lx, size: %lx\n", adm->pa, adm->size);

    return 0;
}
