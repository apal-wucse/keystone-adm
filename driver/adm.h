//******************************************************************************
// Copyright (c) 2023, Akihiro Saiki. All Rights Reserved.
//
// SPDX-License-Identifier: BSD-3-Clause
//------------------------------------------------------------------------------
#ifndef _ADM_H_
#define _ADM_H_

#include <linux/types.h>

#define ADM_SLOT_MAX 16
#define DATA_ALIGN_BYTES 8

typedef struct adm_header {
    uintptr_t adm_top;
    size_t data_n;
    uintptr_t data_offsets[ADM_SLOT_MAX];
    uintptr_t uid_tbl[ADM_SLOT_MAX];
} AdmHeader;

typedef struct adm_region_header {
    uintptr_t offset;
    size_t size;
} AdmRegionHeader;

// use fat pointer for dynamic allocated data
typedef struct adm_ptr {
    uintptr_t uid;
    uintptr_t offset;
} AdmPtr;

typedef enum adm_data_types {
    /* basic types */
    TYPE_NONE, // Dummy
    TYPE_BYTE, // char, uchar
    TYPE_16,   // short, ushort
    TYPE_32,   // int, uint
    TYPE_64,   // long, ulong
    /* array */
    TYPE_ARR_BYTE,
    TYPE_ARR_16,
    TYPE_ARR_32,
    TYPE_ARR_64,
    /* static length array */
    TYPE_STATIC_ARR_BYTE,
    TYPE_STATIC_ARR_16,
    TYPE_STATIC_ARR_32,
    TYPE_STATIC_ARR_64,
    /* dyn alloc region (weaker validation) */
    TYPE_DYN_ALLOC_SPACE,
} AdmDataTypes;

typedef struct adm_type_info {
    unsigned long uid;
    AdmDataTypes type;
    size_t size;
    uintptr_t offset;
} AdmTypeInfo;

#endif
