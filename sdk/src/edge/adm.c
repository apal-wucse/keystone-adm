#include "adm.h"
#include "adm_err.h"
#include "string.h"
#include <stdbool.h>

uintptr_t _adm_start;
size_t _adm_len;
AdmHeader* _adm_hdr;
bool enabled = false;

/* variables for adm malloc */
extern char* __adm_malloc_start;
extern char* __adm_malloc_zone_stop;
extern bool __adm_malloc_initialized;

const char* adm_err_msg[] = {
    [ADM_ERR_SUCCESS]            = "success",
    [ADM_ERR_NOT_AVAILABLE]      = "adm is not available",
    [ADM_ERR_NOSPACE]            = "no space left",
    [ADM_ERR_INVALID_ARGS]       = "invalid arguments",
    [ADM_ERR_UID_NOTFOUND]       = "no region matched with uid",
    [ADM_ERR_OUT_OF_BOUNDS]      = "out of bounds",
    [ADM_ERR_MALLOC_INITIALIZED] = "malloc region was already initialized",
};

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
ADMERR adm_get_region(uintptr_t uid, uintptr_t* ptr, size_t* size) {
    AdmRegionHeader* region_hdr;

    if (!enabled || !_adm_hdr)
        return ADM_ERR_NOT_AVAILABLE;
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

    /* the header and the data must fit after the free list */
    new_offset = _adm_hdr->free_list;
    if (new_offset < sizeof(AdmHeader) || new_offset > _adm_len ||
        _adm_len - new_offset < sizeof(AdmRegionHeader) ||
        size > _adm_len - new_offset - sizeof(AdmRegionHeader)) {
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

ADMERR adm_set_bytes(void* ptr, size_t size, uintptr_t uid) {
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

/* Removes a region.  The other regions stay where they are: the space of a
 * region that is not the last one is free again only once the later ones
 * are removed.  Relies on the layout the SM validates: every header is
 * followed by its data, and the regions are in the order of their index. */
ADMERR adm_remove_region(uintptr_t uid) {
    /* check validity */
    if (!enabled || !_adm_hdr)
        return ADM_ERR_NOT_AVAILABLE;

    size_t n = _adm_hdr->data_n;
    for (size_t i = 0; i < n; i++) {
        /* look for specified uid */
        if (_adm_hdr->uid_tbl[i] != uid)
            continue;

        /* clear the header and the data */
        AdmRegionHeader* hdr = (AdmRegionHeader*)(_adm_start + _adm_hdr->data_offsets[i]);
        memset((void*)hdr, 0, sizeof(AdmRegionHeader) + hdr->size);

        for (size_t j = i; j < n - 1; j++) {
            _adm_hdr->data_offsets[j] = _adm_hdr->data_offsets[j + 1];
            _adm_hdr->uid_tbl[j]      = _adm_hdr->uid_tbl[j + 1];
        }
        _adm_hdr->data_offsets[n - 1] = 0;
        _adm_hdr->uid_tbl[n - 1]      = 0;
        _adm_hdr->data_n              = n - 1;

        if (i == n - 1) { // the tail: free up to the last region that is left
            uintptr_t end = sizeof(AdmHeader);
            if (n > 1) {
                hdr = (AdmRegionHeader*)(_adm_start + _adm_hdr->data_offsets[n - 2]);
                end = ((hdr->offset + hdr->size) / DATA_ALIGN_BYTES + 1) * DATA_ALIGN_BYTES;
            }
            _adm_hdr->free_list = end;
        }
        return ADM_ERR_SUCCESS;
    }

    return ADM_ERR_UID_NOTFOUND;
}

ADMERR adm_reset() {
    if (!enabled || !_adm_hdr) {
        return ADM_ERR_NOT_AVAILABLE;
    }

    /* clear out */
    memset((void*)_adm_start, 0, _adm_len);
    /* set freelist */
    _adm_hdr->free_list = sizeof(AdmHeader);

    return ADM_ERR_SUCCESS;
}

ADMERR adm_dynalloc_init(size_t size) {
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

/* bytes per element of a static length array */
static size_t adm_elem_size(AdmDataTypes type) {
    switch (type) {
    case TYPE_STATIC_ARR_16:
        return 2;
    case TYPE_STATIC_ARR_32:
        return 4;
    case TYPE_STATIC_ARR_64:
        return 8;
    default:
        return 1;
    }
}

/* The size in the metadata of a static length array is its number of elements. */
ADMERR adm_gen_type_info(AdmTypeInfo* type_info, size_t* count) {
    if (!enabled || !_adm_hdr) {
        return ADM_ERR_NOT_AVAILABLE;
    }
    for (size_t i = 0; i < _adm_hdr->data_n; i++) {
        AdmRegionHeader* region_hdr = adm_get_region_header(i);
        type_info[i].uid            = _adm_hdr->uid_tbl[i];
        type_info[i].type           = region_hdr->type;
        type_info[i].size           = region_hdr->size / adm_elem_size(region_hdr->type);
        type_info[i].offset         = region_hdr->offset;
    }
    *count = _adm_hdr->data_n;
    return ADM_ERR_SUCCESS;
}
