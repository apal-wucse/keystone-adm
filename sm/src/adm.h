#ifndef _ADM_H_
#define _ADM_H_

#include <sbi/sbi_types.h>

#define ADM_SLOT_MAX     16
#define DATA_ALIGN_BYTES 8

/* uid for unused slot */
#define ADM_UID_UNUSED 0x00

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

typedef enum adm_protect_type {
    DEFAULT_READ = 0,
    DEFAULT_NOPERM,
} AdmProtect;

typedef struct adm_type_info {
    unsigned long uid;
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

/* pointer type for adm (simple offset val) */
typedef struct adm_ptr {
    uintptr_t offset;
} AdmPtr;

typedef struct adm_state {
    AdmShareTypes share;
    struct adm_type_info type_info[ADM_SLOT_MAX];
} AdmState;

typedef enum adm_validation_error {
    ADM_VALID = 0,
    ADM_INVALID_PARAMS,
    ADM_EXCEEDED_SLOT_SIZE,
    ADM_EXCEEDED_REGION_SIZE,
    ADM_INVALID_ORDER,
    ADM_REGION_OVERLAPPED,
    ADM_OFFSET_MISMATCH,
    ADM_TYPE_BYTE_MISMATCH,
    ADM_TYPE_16_MISMATCH,
    ADM_TYPE_32_MISMATCH,
    ADM_TYPE_64_MISMATCH,
    ADM_TYPE_STATIC_ARR_BYTE_MISMATCH,
    ADM_TYPE_STATIC_ARR_16_MISMATCH,
    ADM_TYPE_STATIC_ARR_32_MISMATCH,
    ADM_TYPE_STATIC_ARR_64_MISMATCH,
    ADM_TYPE_DYN_MISMATCH,
    ADM_INVALID_TYPE,
} AdmValidationError;

extern const char* adm_validation_err_msg[];

#define pr_adm_validation_err(code)                                                                \
    do {                                                                                           \
        sbi_printf("[SM] adm validation: %s\n", adm_validation_err_msg[(code)]);                   \
    } while (0)

AdmValidationError
validate_adm_regions(uintptr_t start_addr, uintptr_t size, struct adm_type_info* type_info);

#endif
