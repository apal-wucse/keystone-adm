//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
// Copyright (c) 2023, Akihiro Saiki. All Rights Reserved.
//
// SPDX-License-Identifier: BSD-3-Clause
//------------------------------------------------------------------------------
// #include <asm/io.h>
// #include <asm/page.h>
#include "keystone.h"

#include <linux/dma-mapping.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "keystone-sbi.h"
#include "keystone_user.h"
#define DRV_DESCRIPTION "Keystone Driver with ADM Support"
#define DRV_VERSION "1.0.0"

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_AUTHOR("Dayeol Lee <dayeol@berkeley.edu>");
MODULE_AUTHOR("Akihiro Saiki <sk@myuu.dev>");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("Dual BSD/GPL");

static const struct file_operations keystone_fops = {
    .owner = THIS_MODULE,
    .mmap = keystone_mmap,
    .unlocked_ioctl = keystone_ioctl,
    .release = keystone_release};

struct miscdevice keystone_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "keystone_enclave",
    .fops = &keystone_fops,
    .mode = 0666,
};

int keystone_mmap(struct file *filp, struct vm_area_struct *vma) {
    struct utm *utm;
    struct epm *epm;
    struct adm *adm;
    struct enclave *enclave;
    unsigned long vsize, psize;
    vaddr_t paddr;
    enclave = get_enclave_by_id((unsigned long)filp->private_data);
    if (!enclave) {
        keystone_err("invalid enclave id: %lu\n", (unsigned long)filp->private_data);
        return -EINVAL;
    }

    utm = enclave->utm;
    epm = enclave->epm;
    adm = enclave->adm;
    vsize = vma->vm_end - vma->vm_start;

    switch (enclave->map_target) {
    case TRUSTED_MEMORY: // EPM
        if (vsize > PAGE_SIZE)
            return -EINVAL;
        paddr = __pa(epm->root_page_table) + (vma->vm_pgoff << PAGE_SHIFT);
        remap_pfn_range(vma, vma->vm_start, paddr >> PAGE_SHIFT, vsize, vma->vm_page_prot);
        break;

    case UNTRUSTED_MEMORY: // UTM
        psize = utm->size;
        if (vsize > psize)
            return -EINVAL;
        remap_pfn_range(vma, vma->vm_start, __pa(utm->ptr) >> PAGE_SHIFT, vsize, vma->vm_page_prot);
        break;

    case PROTECTED_MEMORY: // ADM
        psize = adm->size;
        if (vsize > psize)
            return -EINVAL;
        remap_pfn_range(vma, vma->vm_start, adm->pa >> PAGE_SHIFT, vsize, vma->vm_page_prot);
        break;

    default:
        keystone_err("invalid map target memtype, %d\n", enclave->map_target);
        return -EINVAL;
    }

    return 0;
}

static int __init keystone_dev_init(void) {
    int ret;

    keystone_info("register\n");

    ret = misc_register(&keystone_dev);
    if (ret < 0) {
        keystone_err("driver registration was failed\n");
    }

    keystone_dev.this_device->coherent_dma_mask = DMA_BIT_MASK(32);

    keystone_info(DRV_DESCRIPTION " v" DRV_VERSION "\n");
    return ret;
}

static void __exit keystone_dev_exit(void) {
    keystone_info("unregister\n");
    misc_deregister(&keystone_dev);
    return;
}

module_init(keystone_dev_init);
module_exit(keystone_dev_exit);
