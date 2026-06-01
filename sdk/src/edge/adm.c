#include "adm.h"

#include <stdbool.h>

#include "adm_err.h"
#include "string.h"

uintptr_t _adm_start;
size_t _adm_len;
AdmHeader* _adm_hdr;
bool enabled = false;

/* variables for adm malloc */
extern char* __adm_malloc_start;
extern char* __adm_malloc_zone_stop;
extern bool __adm_malloc_initialized;

void adm_init_internals(uintptr_t start, size_t len) {
    if (start == 0 || len == 0) {
        enabled = false;
        return;
    }
    enabled    = true;
    _adm_start = start;
    _adm_len   = len;
    _adm_hdr   = (AdmHeader*)start;
}

/* retrive region by uid */
ADMERR
adm_get_region(uintptr_t uid, uintptr_t* ptr, size_t* size) {
    AdmRegionHeader* region_hdr;

    if (!enabled || !_adm_hdr)
        return -1;
    for (size_t i = 0; i < _adm_hdr->data_n; i++) {
        if (_adm_hdr->uid_tbl[i] != uid)
            continue; // skip unmatched uid
        region_hdr = adm_get_region_header(i);
        if (!region_hdr)
            return ADM_ERR_OUT_OF_BOUNDS;
        if (ptr != NULL)
            *ptr = (uintptr_t)adm_get_region_base(region_hdr);
        if (size != NULL)
            *size = region_hdr->size;
        return ADM_ERR_SUCCESS;
    }

    return ADM_ERR_UID_NOTFOUND; // specified uid not found
}

static uintptr_t adm_setup_new_hdr(size_t size, uintptr_t uid, AdmDataTypes type) {
    uintptr_t new_offset, region;
    AdmRegionHeader* hdr;

    /* over capacity */
    if (!enabled || !_adm_hdr || _adm_hdr->data_n >= ADM_SLOT_MAX) {
        return (uintptr_t)NULL;
    }
    /* check uid */
    for (size_t i = 0; i < _adm_hdr->data_n; i++) {
        if (_adm_hdr->uid_tbl[i] == uid)
            return (uintptr_t)NULL;
    }

    new_offset = _adm_hdr->free_list;
    if (new_offset > UINTPTR_MAX - _adm_start || new_offset >= _adm_len)
        return (uintptr_t)NULL;
    if (size > UINTPTR_MAX - (_adm_start + new_offset + sizeof(AdmRegionHeader)) ||
        size > _adm_len - (new_offset + sizeof(AdmRegionHeader))) {
        return (uintptr_t)NULL;
    }

    _adm_hdr->data_n++;
    _adm_hdr->data_offsets[_adm_hdr->data_n - 1] = new_offset;
    _adm_hdr->uid_tbl[_adm_hdr->data_n - 1]      = uid;

    hdr         = (AdmRegionHeader*)(_adm_start + new_offset);
    region      = new_offset + sizeof(AdmRegionHeader);
    hdr->offset = region;
    hdr->size   = size;
    hdr->type   = type;

    /* next offset */
    _adm_hdr->free_list = ((region + size) / DATA_ALIGN_BYTES + 1) * DATA_ALIGN_BYTES;

    return region;
}

ADMERR
adm_set_bytes(void* ptr, size_t size, uintptr_t uid) {
    uintptr_t data_offset;

    if (!ptr || size == 0) {
        return ADM_ERR_INVALID_ARGS;
    }

    data_offset = adm_setup_new_hdr(size, uid, TYPE_STATIC_ARR_BYTE);

    if (!data_offset) {
        return ADM_ERR_NOSPACE;
    }

    /* copy data buffer into the region */
    memcpy((void*)(_adm_start + data_offset), ptr, size);

    return ADM_ERR_SUCCESS;
}

void* adm_new_region(size_t size, uintptr_t uid, AdmDataTypes type) {
    uintptr_t data_offset;
    data_offset = adm_setup_new_hdr(size, uid, type);
    if (!data_offset)
        return NULL;
    return (void*)(_adm_start + data_offset);
}

/* region removing & relocating */
ADMERR
adm_remove_region(uintptr_t uid) {
    /* check validity */
    if (!enabled || !_adm_hdr)
        return ADM_ERR_NOT_AVAILABLE;

    for (size_t i = 0; i < _adm_hdr->data_n; i++) {
        /* look for specified uid */
        if (_adm_hdr->uid_tbl[i] != uid)
            continue;

        if (i < _adm_hdr->data_n - 1) { // number of successor >= 1
            uintptr_t offset_gap       = _adm_hdr->data_offsets[i + 1] - _adm_hdr->data_offsets[i];
            uintptr_t copy_dst         = _adm_start + _adm_hdr->data_offsets[i];
            uintptr_t copy_src         = _adm_start + _adm_hdr->data_offsets[i + 1];
            AdmRegionHeader* last_data = adm_get_region_header(_adm_hdr->data_n - 1);
            uintptr_t before_end       = last_data->offset + last_data->size;
            uintptr_t copy_size        = before_end - _adm_hdr->data_offsets[i + 1];
            uintptr_t new_free_list =
                ((before_end - offset_gap) / DATA_ALIGN_BYTES + 1) * DATA_ALIGN_BYTES;

            /* relocate region header */
            for (size_t j = i; j < _adm_hdr->data_n - 1; j++) {
                _adm_hdr->data_offsets[j] = _adm_hdr->data_offsets[j + 1] - offset_gap;
                _adm_hdr->uid_tbl[j]      = _adm_hdr->uid_tbl[j + 1];
            }

            /* clear trailing metadata */
            _adm_hdr->data_offsets[_adm_hdr->data_n - 1] = 0;
            _adm_hdr->uid_tbl[_adm_hdr->data_n - 1]      = 0;
            _adm_hdr->data_n--;
            _adm_hdr->free_list = new_free_list;

            /* relocate region */
            memcpy((void*)copy_dst, (void*)copy_src, copy_size);

            /* clear old region */
            memset((void*)(_adm_start + before_end - offset_gap), 0, offset_gap);
        } else { // target is the tail
            uintptr_t remove_base        = _adm_start + _adm_hdr->data_offsets[i];
            AdmRegionHeader* remove_data = (AdmRegionHeader*)remove_base;
            uintptr_t remove_size =
                remove_data->offset + remove_data->size - _adm_hdr->data_offsets[i];

            /* clear out */
            _adm_hdr->free_list       = _adm_hdr->data_offsets[i];
            _adm_hdr->data_offsets[i] = 0;
            _adm_hdr->uid_tbl[i]      = 0;
            _adm_hdr->data_n--;

            memset((void*)remove_base, 0, remove_size);
        }
        return ADM_ERR_SUCCESS;
    }

    return ADM_ERR_UID_NOTFOUND;
}

ADMERR
adm_reset() {
    if (!enabled || !_adm_hdr) {
        return ADM_ERR_NOT_AVAILABLE;
    }

    /* clear out */
    memset((void*)_adm_start, 0, _adm_len);
    /* set freelist */
    _adm_hdr->free_list = (uintptr_t)(_adm_start + sizeof(AdmHeader));

    return ADM_ERR_SUCCESS;
}

ADMERR
adm_dynalloc_init(size_t size) {
    if (__adm_malloc_initialized)
        return ADM_ERR_MALLOC_INITIALIZED; // already initialized
    uintptr_t ptr = 0;
    size_t _size;

    adm_get_region(ADM_UID_MALLOC_SPACE, &ptr, &_size);
    if (ptr && size != _size)
        return ADM_ERR_NOSPACE;

    if (!ptr) {
        ptr = (uintptr_t)adm_new_region(size, ADM_UID_MALLOC_SPACE, TYPE_DYN_ALLOC_SPACE);
        if (!ptr)
            return ADM_ERR_NOSPACE;
    }

    memset((void*)ptr, 0, size);

    __adm_malloc_start       = (char*)ptr;
    __adm_malloc_zone_stop   = (char*)(ptr + size);
    __adm_malloc_initialized = true;

    return ADM_ERR_SUCCESS;
}

bool adm_region_exists(uintptr_t uid) {
    for (size_t i = 0; i < _adm_hdr->data_n; i++) {
        if (_adm_hdr->uid_tbl[i] == uid)
            return true;
    }
    return false;
}

ADMERR
adm_gen_type_info(AdmTypeInfo* type_info, size_t* count) {
    if (!enabled || !_adm_hdr) {
        return ADM_ERR_NOT_AVAILABLE;
    }
    for (size_t i = 0; i < _adm_hdr->data_n; i++) {
        AdmRegionHeader* region_hdr = adm_get_region_header(i);
        type_info[i].uid            = _adm_hdr->uid_tbl[i];
        type_info[i].type           = region_hdr->type;
        type_info[i].size           = region_hdr->size;
    }
    *count = _adm_hdr->data_n;
    return ADM_ERR_SUCCESS;
}
