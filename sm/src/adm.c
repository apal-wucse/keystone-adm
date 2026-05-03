#include "adm.h"
#include "mprv.h"
#include "sm.h"
#include <stdint.h>

int validate_adm_regions(uintptr_t start_addr, uintptr_t size, AdmTypeInfo *type_info) {
    /* Parameter Validation */
    if (start_addr == 0 || size == 0 || type_info == NULL)
        return -1;
    /* ADM Region Info */
    AdmHeader *info = (AdmHeader *)start_addr;
    if (info->data_n > ADM_SLOT_MAX)
        return -1;
    /* Validation */
    for (int i = 0; i < info->data_n; i++) {
        if (info->data_offsets[i] >= size) // must not exceed region
            return -1;
        if (i < info->data_n - 1 &&
            info->data_offsets[i] >= info->data_offsets[i + 1]) // must be increasing order
            return -1;

        uintptr_t hdr_offset = info->data_offsets[i];
        if (hdr_offset + sizeof(AdmRegionHeader) > size)
            return -1;
        AdmRegionHeader *data = (AdmRegionHeader *)(start_addr + hdr_offset);

        if (data->offset + data->size > size) // must not exceed region
            return -1;
        if (i < info->data_n - 1 &&
            data->offset + data->size > info->data_offsets[i + 1]) // must not overlap successor
            return -1;

        AdmDataTypes type = TYPE_NONE;
        size_t size = 0;
        uintptr_t offset = 0;
        for (size_t j = 0; j < ADM_SLOT_MAX; j++) { // search for uid
            if (type_info[j].uid == ADM_UID_UNUSED)
                continue;
            if (info->uid_tbl[i] == type_info[j].uid) { // pick up first match
                type = type_info[j].type;
                size = type_info[j].size;
                offset = type_info[j].offset;
                break;
            }
        }

        if (data->offset != offset)
            return -1;

        switch (type) {
        case TYPE_BYTE:
            if (data->size != 1)
                return -1;
            break;
        case TYPE_16:
            if (data->size != 2)
                return -1;
            break;
        case TYPE_32:
            if (data->size != 4)
                return -1;
            break;
        case TYPE_64:
            if (data->size != 8)
                return -1;
            break;
        case TYPE_STATIC_ARR_BYTE:
            if (data->size != size)
                return -1;
            break;
        case TYPE_STATIC_ARR_16:
            if (data->size != size * 2)
                return -1;
            break;
        case TYPE_STATIC_ARR_32:
            if (data->size != size * 4)
                return -1;
            break;
        case TYPE_STATIC_ARR_64:
            if (data->size != size * 8)
                return -1;
            break;
        case TYPE_ARR_BYTE:
        case TYPE_ARR_16:
        case TYPE_ARR_32:
        case TYPE_ARR_64:
            // bypass size check for variable size array
            break;
        case TYPE_DYN_ALLOC_SPACE:
            if (data->size != size)
                return -1;
            break;
        case TYPE_NONE:
            // No type info
            // unexpected data received
            return -1;
        default:
            // fallback
            return -1;
        }
    }
    return 0;
}
