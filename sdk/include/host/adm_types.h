#ifndef _ADM_TYPES_H_
#define _ADM_TYPES_H_

#ifndef _ADM_H_

#include <stddef.h>
#include <stdint.h>

#define ADM_SLOT_MAX     16
#define DATA_ALIGN_BYTES 8

/* for validation */
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
    /* dyn alloc region (weak validation) */
    TYPE_DYN_ALLOC_SPACE,
} AdmDataTypes;

/* header of overall mememory */
typedef struct adm_header {
    size_t data_n;
    uintptr_t free_list;
    uintptr_t data_offsets[ADM_SLOT_MAX];
    uintptr_t uid_tbl[ADM_SLOT_MAX];
} AdmHeader;

/* header of chunks */
typedef struct adm_region_header {
    AdmDataTypes type;
    uintptr_t offset;
    size_t size;
} AdmRegionHeader;

/* for validation */
typedef struct adm_type_info {
    uintptr_t uid;
    AdmDataTypes type;
    size_t size;
    uintptr_t offset;
} AdmTypeInfo;

/* permission identifier */
typedef enum adm_share_types {
    ADM_SHARE_NONE = 0,
    ADM_SHARE_RONLY,
    ADM_SHARE_RW,
} AdmShareTypes;

#endif

typedef enum ProtectionTypes {
    READABLE,
    STRICT,
} ProtectionTypes;

#endif
