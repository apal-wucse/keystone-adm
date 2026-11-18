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
    [ADM_REGION_COUNT_MISMATCH]         = "number of regions is mismatched",
    [ADM_INVALID_UID]                   = "region uid is unknown or duplicated",
    [ADM_TYPE_MISMATCH]                 = "region type is mismatched",
    [ADM_HEADER_NOT_ADJACENT]           = "region header is not followed by its data",
    [ADM_MISALIGNED]                    = "region header is misaligned",
    [ADM_INVALID_FREE_LIST]             = "invalid free list",
};

/* [a, a + l) and [b, b + m) share a byte; a + l and b + m must not overflow */
static int overlaps(uintptr_t a, uintptr_t l, uintptr_t b, uintptr_t m) {
    return l > 0 && m > 0 && a < b + m && b < a + l;
}

/* d: size of the data body, s: size in the metadata */
static AdmValidationError check_size(AdmDataTypes type, size_t d, size_t s) {
    switch (type) {
    case TYPE_BYTE:
        return d == 1 ? ADM_VALID : ADM_TYPE_BYTE_MISMATCH;
    case TYPE_16:
        return d == 2 ? ADM_VALID : ADM_TYPE_16_MISMATCH;
    case TYPE_32:
        return d == 4 ? ADM_VALID : ADM_TYPE_32_MISMATCH;
    case TYPE_64:
        return d == 8 ? ADM_VALID : ADM_TYPE_64_MISMATCH;
    case TYPE_STATIC_ARR_BYTE:
        return d == s ? ADM_VALID : ADM_TYPE_STATIC_ARR_BYTE_MISMATCH;
    case TYPE_STATIC_ARR_16:
        return d % 2 == 0 && d / 2 == s ? ADM_VALID : ADM_TYPE_STATIC_ARR_16_MISMATCH;
    case TYPE_STATIC_ARR_32:
        return d % 4 == 0 && d / 4 == s ? ADM_VALID : ADM_TYPE_STATIC_ARR_32_MISMATCH;
    case TYPE_STATIC_ARR_64:
        return d % 8 == 0 && d / 8 == s ? ADM_VALID : ADM_TYPE_STATIC_ARR_64_MISMATCH;
    case TYPE_ARR_BYTE:
    case TYPE_ARR_16:
    case TYPE_ARR_32:
    case TYPE_ARR_64:
        // variable size array: range checks only
        return ADM_VALID;
    case TYPE_DYN_ALLOC_SPACE:
        return d == s ? ADM_VALID : ADM_TYPE_DYN_MISMATCH;
    case TYPE_NONE:
    default:
        return ADM_INVALID_TYPE;
    }
}

/* Each field of the ADM is read once. */
AdmValidationError
validate_adm_regions(uintptr_t start_addr, uintptr_t size, AdmTypeInfo* type_info) {
    const uintptr_t hdr_size = sizeof(AdmHeader);
    const uintptr_t rhdr     = sizeof(AdmRegionHeader);
    uintptr_t hoff[ADM_SLOT_MAX], uid[ADM_SLOT_MAX], doff[ADM_SLOT_MAX], dsize[ADM_SLOT_MAX];
    uintptr_t free_list, end = hdr_size;
    AdmValidationError ret;
    size_t n, expected = 0;

    if (start_addr == 0 || size < hdr_size || type_info == NULL)
        return ADM_INVALID_PARAMS;

    AdmHeader* info = (AdmHeader*)start_addr;
    n               = info->data_n;
    if (n > ADM_SLOT_MAX)
        return ADM_EXCEEDED_SLOT_SIZE;

    /* every expected region is present exactly once */
    for (size_t j = 0; j < ADM_SLOT_MAX; j++)
        if (type_info[j].uid != ADM_UID_UNUSED)
            expected++;
    if (n != expected)
        return ADM_REGION_COUNT_MISMATCH;

    for (size_t i = 0; i < n; i++) {
        AdmTypeInfo* t = NULL;

        hoff[i] = info->data_offsets[i];
        uid[i]  = info->uid_tbl[i];

        /* region header: after the ADM header, inside the ADM, aligned, after the previous one */
        if (hoff[i] < hdr_size || hoff[i] > size - rhdr)
            return ADM_EXCEEDED_REGION_SIZE;
        if (hoff[i] % DATA_ALIGN_BYTES != 0)
            return ADM_MISALIGNED;
        if (i > 0 && hoff[i] < hoff[i - 1] + rhdr)
            return ADM_INVALID_ORDER;

        for (size_t j = 0; j < i; j++)
            if (uid[j] == uid[i])
                return ADM_INVALID_UID;
        for (size_t j = 0; j < ADM_SLOT_MAX; j++) {
            if (type_info[j].uid != ADM_UID_UNUSED && type_info[j].uid == uid[i]) {
                t = &type_info[j];
                break;
            }
        }
        if (t == NULL)
            return ADM_INVALID_UID;

        AdmRegionHeader* data = (AdmRegionHeader*)(start_addr + hoff[i]);
        AdmDataTypes type     = data->type;
        doff[i]               = data->offset;
        dsize[i]              = data->size;

        /* data body: right after its header, inside the ADM */
        if (doff[i] != hoff[i] + rhdr)
            return ADM_HEADER_NOT_ADJACENT;
        if (dsize[i] > size || doff[i] > size - dsize[i])
            return ADM_EXCEEDED_REGION_SIZE;
        if (overlaps(0, hdr_size, doff[i], dsize[i]))
            return ADM_REGION_OVERLAPPED;

        if (type != t->type)
            return ADM_TYPE_MISMATCH;
        if (doff[i] != t->offset)
            return ADM_OFFSET_MISMATCH;
        ret = check_size(type, dsize[i], t->size);
        if (ret != ADM_VALID)
            return ret;
    }

    /* a data body overlaps no region header and no other data body */
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            if (overlaps(doff[i], dsize[i], hoff[j], rhdr))
                return ADM_REGION_OVERLAPPED;
            if (j < i && overlaps(doff[i], dsize[i], doff[j], dsize[j]))
                return ADM_REGION_OVERLAPPED;
        }
    }

    /* the free list: aligned, after the last region (the regions are in order) */
    free_list = info->free_list;
    if (n > 0)
        end = doff[n - 1] + dsize[n - 1];
    if (free_list < end || free_list % DATA_ALIGN_BYTES != 0)
        return ADM_INVALID_FREE_LIST;
    return ADM_VALID;
}
