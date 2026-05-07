//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
// Copyright (c) 2023, Akihiro Saiki. All Rights Reserved.
//
// SPDX-License-Identifier: BSD-3-Clause
//------------------------------------------------------------------------------
#include <asm/sbi.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "keystone-sbi.h"
#include "keystone.h"
#include "keystone_user.h"

int __keystone_destroy_enclave(unsigned int ueid);

int keystone_create_enclave(struct file *filep, unsigned long arg) {
    /* create parameters */
    struct keystone_ioctl_create_enclave *enclp = (struct keystone_ioctl_create_enclave *)arg;

    struct enclave *enclave;
    enclave = create_enclave(enclp->min_pages);

    if (enclave == NULL) {
        return -ENOMEM;
    }

    /* Pass base page table */
    enclp->pt_ptr = __pa(enclave->epm->root_page_table);
    enclp->epm_size = enclave->epm->size;

    /* allocate UID */
    enclp->eid = enclave_idr_alloc(enclave);

    filep->private_data = (void *)enclp->eid;

    return 0;
}

int keystone_finalize_enclave(unsigned long arg) {
    struct sbiret ret;
    struct enclave *enclave;
    struct utm *utm;
    struct adm *adm;
    struct keystone_sbi_create_t create_args;

    struct keystone_ioctl_create_enclave *enclp = (struct keystone_ioctl_create_enclave *)arg;

    enclave = get_enclave_by_id(enclp->eid);
    if (!enclave) {
        keystone_err("invalid enclave id: %lu\n", enclp->eid);
        return -EINVAL;
    }

    // After finalize, can access only untrusted memory
    // ADM has read-only access
    enclave->map_target = UNTRUSTED_MEMORY;

    /* SBI Call */
    create_args.epm_region.paddr = enclave->epm->pa;
    create_args.epm_region.size = enclave->epm->size;

    utm = enclave->utm;

    if (utm) {
        create_args.utm_region.paddr = __pa(utm->ptr);
        create_args.utm_region.size = utm->size;
    } else {
        create_args.utm_region.paddr = 0;
        create_args.utm_region.size = 0;
    }

    adm = enclave->adm;

    if (enclave->adm_enabled && adm) {
        create_args.adm_region.paddr = adm->pa;
        create_args.adm_region.size = adm->size;
        create_args.adm_protection = adm->protection;
    } else {
        create_args.adm_region.paddr = 0;
        create_args.adm_region.size = 0;
        create_args.adm_protection = ADM_DEFAULT_NOPERM;
    }

    // physical addresses for runtime, user, and freemem
    create_args.runtime_paddr = enclp->runtime_paddr;
    create_args.user_paddr = enclp->user_paddr;
    create_args.free_paddr = enclp->free_paddr;

    create_args.params = enclp->params;

    if (enclave->adm_enabled) {
        for (int i = 0; i < ADM_SLOT_MAX; i++) {
            create_args.type_info[i] = enclp->type_info[i];
        }
    }

    ret = sbi_sm_create_enclave(&create_args);

    if (ret.error) {
        keystone_err("create_enclave: SBI error code %ld\n", ret.error);
        goto error_destroy_enclave;
    }

    enclave->eid = ret.value;

    return 0;

error_destroy_enclave:
    /* This can handle partial initialization failure */
    destroy_enclave(enclave);

    return -EINVAL;
}

int keystone_run_enclave(unsigned long data) {
    struct sbiret ret;
    unsigned long ueid;
    struct enclave *enclave;
    struct keystone_ioctl_run_enclave *arg = (struct keystone_ioctl_run_enclave *)data;

    ueid = arg->eid;
    enclave = get_enclave_by_id(ueid);

    if (!enclave) {
        keystone_err("invalid enclave id: %lu\n", ueid);
        return -EINVAL;
    }

    if (enclave->eid < 0) {
        keystone_err("enclave was defined but does not exist, id: %lu\n", ueid);
        return -EINVAL;
    }

    ret = sbi_sm_run_enclave(enclave->eid);

    arg->error = ret.error;
    arg->value = ret.value;

    return 0;
}

int utm_init_ioctl(struct file *filp, unsigned long arg) {
    int ret = 0;
    struct utm *utm;
    struct enclave *enclave;
    struct keystone_ioctl_memalloc *enclp = (struct keystone_ioctl_memalloc *)arg;
    long long unsigned untrusted_size = enclp->size;

    enclave = get_enclave_by_id(enclp->eid);

    if (!enclave) {
        keystone_err("invalid enclave id: %lu\n", enclp->eid);
        return -EINVAL;
    }

    utm = kmalloc(sizeof(struct utm), GFP_KERNEL);
    if (!utm) {
        ret = -ENOMEM;
        return ret;
    }

    ret = utm_init(utm, untrusted_size);

    /* prepare for mmap */
    enclave->utm = utm;

    enclp->paddr = __pa(utm->ptr);
    enclp->alloc_size = utm->size;

    return ret;
}

int adm_init_ioctl(struct file *filep, unsigned long arg) {
    int ret = 0;
    struct adm *adm;
    struct enclave *enclave;
    struct keystone_ioctl_memalloc *enclp = (struct keystone_ioctl_memalloc *)arg;
    long long unsigned additional_size = enclp->size;

    enclave = get_enclave_by_id(enclp->eid);

    if (!enclave) {
        keystone_err("invalid enclave id: %lu\n");
        return -EINVAL;
    }

    adm = kmalloc(sizeof(struct adm), GFP_KERNEL);
    if (!adm) {
        keystone_err("failed to allocate kernel mem\n");
        ret = -ENOMEM;
        return ret;
    }

    ret = adm_init(adm, additional_size, enclp->protect_type);

    enclave->adm_enabled = true;
    enclave->adm = adm;

    enclp->paddr = adm->pa;
    enclp->alloc_size = adm->size;

    return ret;
}

int keystone_destroy_enclave(struct file *filep, unsigned long arg) {
    int ret;
    struct keystone_ioctl_create_enclave *enclp = (struct keystone_ioctl_create_enclave *)arg;
    unsigned long ueid = enclp->eid;

    ret = __keystone_destroy_enclave(ueid);
    if (!ret) {
        filep->private_data = NULL;
    }
    return ret;
}

int __keystone_destroy_enclave(unsigned int ueid) {
    struct sbiret ret;
    struct enclave *enclave;
    enclave = get_enclave_by_id(ueid);

    if (!enclave) {
        keystone_err("invalid enclave id: %lu\n", ueid);
        return -EINVAL;
    }

    if (enclave->eid >= 0) {
        ret = sbi_sm_destroy_enclave(enclave->eid);
        if (ret.error) {
            keystone_err(
                "fatal: cannot destroy enclave: SBI error code "
                "%ld\n",
                ret.error);
            return -EINVAL;
        }
    } else {
        keystone_warn("destroy_enclave: skipping (enclave does not exist)\n");
    }

    destroy_enclave(enclave);
    enclave_idr_remove(ueid);

    return 0;
}

int keystone_resume_enclave(unsigned long data) {
    struct sbiret ret;
    struct keystone_ioctl_run_enclave *arg = (struct keystone_ioctl_run_enclave *)data;
    unsigned long ueid = arg->eid;
    struct enclave *enclave;
    enclave = get_enclave_by_id(ueid);

    if (!enclave) {
        keystone_err("invalid enclave id: %lu\n", ueid);
        return -EINVAL;
    }

    if (enclave->eid < 0) {
        keystone_err("enclave was defined but does not exist, id: %lu\n", ueid);
        return -EINVAL;
    }

    ret = sbi_sm_resume_enclave(enclave->eid);

    arg->error = ret.error;
    arg->value = ret.value;

    return 0;
}

int keystone_swap_map(unsigned long data) {
    struct keystone_ioctl_memalloc *arg = (struct keystone_ioctl_memalloc *)data;
    unsigned long ueid = arg->eid;
    struct enclave *enclave;
    enclave = get_enclave_by_id(ueid);

    if (!enclave) {
        keystone_err("invalid enclave id: %lu\n", ueid);
        return -EINVAL;
    }

    if (enclave->eid < 0) {
        keystone_err("enclave was defined but does not exist, id: %lu\n", ueid);
        return -EINVAL;
    }

    if (arg->mem_type > 2) {
        keystone_err("invalid memtype: %u\n", arg->mem_type);
        return -EINVAL;
    }

    enclave->map_target = arg->mem_type;

    return 0;
}

long keystone_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {
    long ret;
    char data[768];

    size_t ioc_size;

    if (!arg)
        return -EINVAL;

    ioc_size = _IOC_SIZE(cmd);
    ioc_size = ioc_size > sizeof(data) ? sizeof(data) : ioc_size;

    if (copy_from_user(data, (void __user *)arg, ioc_size))
        return -EFAULT;

    switch (cmd) {
    case KEYSTONE_IOC_CREATE_ENCLAVE:
        ret = keystone_create_enclave(filep, (unsigned long)data);
        break;
    case KEYSTONE_IOC_FINALIZE_ENCLAVE:
        ret = keystone_finalize_enclave((unsigned long)data);
        break;
    case KEYSTONE_IOC_DESTROY_ENCLAVE:
        ret = keystone_destroy_enclave(filep, (unsigned long)data);
        break;
    case KEYSTONE_IOC_RUN_ENCLAVE:
        ret = keystone_run_enclave((unsigned long)data);
        break;
    case KEYSTONE_IOC_RESUME_ENCLAVE:
        ret = keystone_resume_enclave((unsigned long)data);
        break;

    /* Note that following commands could have been implemented as a part of
     * ADD_PAGE ioctl. However, there was a weird bug in compiler that
     * generates a wrong control flow that ends up with an illegal
     * instruction if we combine switch-case and if statements. We didn't
     * identified the exact problem, so we'll have these until we figure out
     */
    case KEYSTONE_IOC_UTM_INIT:
        ret = utm_init_ioctl(filep, (unsigned long)data);
        break;

    case KEYSTONE_IOC_ADM_INIT:
        ret = adm_init_ioctl(filep, (unsigned long)data);
        break;

    /* this ioctl set false to pte initialization flag
     * after this, mmap allocate adm region for userspace
     */
    case KEYSTONE_IOC_SWAP_MAP:
        ret = keystone_swap_map((unsigned long)data);
        break;

    default:
        return -ENOSYS;
    }

    if (copy_to_user((void __user *)arg, data, ioc_size))
        return -EFAULT;

    return ret;
}

int keystone_release(struct inode *inode, struct file *file) {
    unsigned long ueid = (unsigned long)(file->private_data);
    struct enclave *enclave;

    /* enclave has been already destroyed */
    if (!ueid) {
        return 0;
    }

    /* We need to send destroy enclave just the eid to close. */
    enclave = get_enclave_by_id(ueid);

    if (!enclave) {
        /* If eid is set to the invalid id, then we do not do anything. */
        return -EINVAL;
    }
    if (enclave->close_on_pexit) {
        return __keystone_destroy_enclave(ueid);
    }
    return 0;
}
