//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
// Copyright (c) 2023, Akihiro Saiki. All Rights Reserved.
//
// SPDX-License-Identifier: BSD-3-Clause
//------------------------------------------------------------------------------
#ifndef _KEYSTONE_SBI_
#define _KEYSTONE_SBI_

#include <asm/sbi.h>

#include "keystone_user.h"

#define KEYSTONE_SBI_EXT_ID 0x08424b45
#define SBI_SM_CREATE_ENCLAVE 2001
#define SBI_SM_DESTROY_ENCLAVE 2002
#define SBI_SM_RUN_ENCLAVE 2003
#define SBI_SM_RESUME_ENCLAVE 2005

struct keystone_sbi_pregion_t {
    uintptr_t paddr;
    size_t size;
};

struct keystone_sbi_create_t {
    // Memory regions for the enclave
    struct keystone_sbi_pregion_t epm_region;
    struct keystone_sbi_pregion_t utm_region;
    struct keystone_sbi_pregion_t adm_region;

    // physical addresses
    uintptr_t runtime_paddr;
    uintptr_t user_paddr;
    uintptr_t free_paddr;

    // Parameters
    struct runtime_params_t params;
    unsigned int *eid_pptr; // TODO: remove?

    AdmTypeInfo type_info[ADM_SLOT_MAX];
    __u8 adm_protection;
};

struct sbiret sbi_sm_create_enclave(struct keystone_sbi_create_t *args);
struct sbiret sbi_sm_destroy_enclave(unsigned long eid);
struct sbiret sbi_sm_run_enclave(unsigned long eid);
struct sbiret sbi_sm_resume_enclave(unsigned long eid);

#endif
