#include "adm.h"
#include "mprv.h"
#include "sm.h"
#include <stdint.h>

const char* adm_validation_err_msg[] = {
    [ADM_VALID]                         = "ADM is valid",
    [ADM_INVALID_PARAMS]                = "invalid parameters",
    [ADM_EXCEEDED_SLOT_SIZE]            = "exceeded maximum number of slot",
    [ADM_EXCEEDED_REGION_SIZE]          = "exceeded ADM regions size",
    [ADM_INVALID_ORDER]                 = "invalid region order",
    [ADM_REGION_OVERLAPPED]             = "region are overlapped",
    [ADM_OFFSET_MISMATCH]               = "data offset is mismatched",
    [ADM_TYPE_BYTE_MISMATCH]            = "data size is mismatched with TYPE_BYTE",
    [ADM_TYPE_16_MISMATCH]              = "data size is mismatched with TYPE_16",
    [ADM_TYPE_32_MISMATCH]              = "data size is mismatched with TYPE_32",
    [ADM_TYPE_64_MISMATCH]              = "data size is mismatched with TYPE_64",
    [ADM_TYPE_STATIC_ARR_BYTE_MISMATCH] = "data size is mismatched with TYPE_STATIC_ARR_BYTE",
    [ADM_TYPE_STATIC_ARR_16_MISMATCH]   = "data size is mismatched with TYPE_STATIC_ARR_16",
    [ADM_TYPE_STATIC_ARR_32_MISMATCH]   = "data size is mismatched with TYPE_STATIC_ARR_32",
    [ADM_TYPE_STATIC_ARR_64_MISMATCH]   = "data size is mismatched with TYPE_STATIC_ARR_64",
    [ADM_TYPE_DYN_MISMATCH]             = "dynamic region size is mismatched",
    [ADM_INVALID_TYPE]                  = "invalid region type",
};

AdmValidationError
validate_adm_regions(uintptr_t start_addr, uintptr_t size, AdmTypeInfo* type_info) {
    /* Parameter Validation */
    if (start_addr == 0 || size == 0 || type_info == NULL)
        return ADM_INVALID_PARAMS;
    /* ADM Region Info */
    AdmHeader* info = (AdmHeader*)start_addr;
    if (info->data_n > ADM_SLOT_MAX)
        return ADM_EXCEEDED_SLOT_SIZE;
    /* Validation */
    for (int i = 0; i < info->data_n; i++) {
        if (info->data_offsets[i] >= size) // must not exceed region
            return ADM_EXCEEDED_REGION_SIZE;
        if (i < info->data_n - 1 &&
            info->data_offsets[i] >= info->data_offsets[i + 1]) // must be increasing order
            return ADM_INVALID_ORDER;

        uintptr_t hdr_offset = info->data_offsets[i];
        if (hdr_offset + sizeof(AdmRegionHeader) > size)
            return ADM_EXCEEDED_REGION_SIZE;
        AdmRegionHeader* data = (AdmRegionHeader*)(start_addr + hdr_offset);

        if (data->offset + data->size > size) // must not exceed region
            return ADM_EXCEEDED_REGION_SIZE;
        if (i < info->data_n - 1 &&
            data->offset + data->size > info->data_offsets[i + 1]) // must not overlap successor
            return ADM_REGION_OVERLAPPED;

        AdmDataTypes type = TYPE_NONE;
        size_t size       = 0;
        uintptr_t offset  = 0;
        for (size_t j = 0; j < ADM_SLOT_MAX; j++) { // search for uid
            if (type_info[j].uid == ADM_UID_UNUSED)
                continue;
            if (info->uid_tbl[i] == type_info[j].uid) { // pick up first match
                type   = type_info[j].type;
                size   = type_info[j].size;
                offset = type_info[j].offset;
                break;
            }
        }

        if (data->offset != offset)
            return ADM_OFFSET_MISMATCH;

        switch (type) {
        case TYPE_BYTE:
            if (data->size != 1)
                return ADM_TYPE_BYTE_MISMATCH;
            break;
        case TYPE_16:
            if (data->size != 2)
                return ADM_TYPE_16_MISMATCH;
            break;
        case TYPE_32:
            if (data->size != 4)
                return ADM_TYPE_32_MISMATCH;
            break;
        case TYPE_64:
            if (data->size != 8)
                return ADM_TYPE_64_MISMATCH;
            break;
        case TYPE_STATIC_ARR_BYTE:
            if (data->size != size)
                return ADM_TYPE_STATIC_ARR_BYTE_MISMATCH;
            break;
        case TYPE_STATIC_ARR_16:
            if (data->size != size * 2)
                return ADM_TYPE_STATIC_ARR_16_MISMATCH;
            break;
        case TYPE_STATIC_ARR_32:
            if (data->size != size * 4)
                return ADM_TYPE_STATIC_ARR_32_MISMATCH;
            break;
        case TYPE_STATIC_ARR_64:
            if (data->size != size * 8)
                return ADM_TYPE_STATIC_ARR_64_MISMATCH;
            break;
        case TYPE_ARR_BYTE:
        case TYPE_ARR_16:
        case TYPE_ARR_32:
        case TYPE_ARR_64:
            // bypass size check for variable size array
            break;
        case TYPE_DYN_ALLOC_SPACE:
            if (data->size != size)
                return ADM_TYPE_DYN_MISMATCH;
            break;
        case TYPE_NONE:
        default:
            // No type info
            // unexpected data received
            return ADM_INVALID_TYPE;
        }
    }
    return ADM_VALID;
}
