#ifndef _ADM_H_
#define _ADM_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "adm_err.h"

/* uid for unused slot */
#define ADM_UID_UNUSED 0x00

/* UID for malloc space
   now supports single malloc region */
#define ADM_UID_MALLOC_SPACE 0xff80

#ifndef _ADM_TYPES_H_

#define ADM_SLOT_MAX 16     // maximum number of region slots
#define DATA_ALIGN_BYTES 8  // alignment

/* for validation */
typedef enum adm_data_types {
  /* basic types */
  TYPE_NONE,  // Dummy
  TYPE_BYTE,  // char, uchar
  TYPE_16,    // short, ushort
  TYPE_32,    // int, uint
  TYPE_64,    // long, ulong
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

#endif /* _ADM_TYPES_H_ */

/* pointer type for adm (simple offset val) */
typedef struct adm_ptr {
  uintptr_t offset;
} AdmPtr;

#ifdef __cplusplus
extern "C" {
#endif

extern uintptr_t _adm_start;
extern size_t _adm_len;
extern AdmHeader* _adm_hdr;

void
adm_init_internals(uintptr_t start, size_t len);

ADMERR
adm_get_region(uintptr_t uid, uintptr_t* ptr, size_t* size);

ADMERR
adm_set_bytes(void* ptr, size_t size, uintptr_t uid);

void*
adm_new_region(size_t size, uintptr_t uid, AdmDataTypes type);

ADMERR
adm_remove_region(uintptr_t uid);

ADMERR
adm_reset();

ADMERR
adm_dynalloc_init(size_t size);

bool
adm_region_exists(uintptr_t uid);

ADMERR
adm_gen_type_info(AdmTypeInfo* type_info, size_t* count);

static inline AdmHeader*
adm_get_header() {
  return _adm_hdr;
}

static inline AdmRegionHeader*
adm_get_region_header(size_t idx) {
  uintptr_t offset = _adm_hdr->data_offsets[idx];
  if (offset == 0 || offset > UINTPTR_MAX - _adm_start || offset >= _adm_len)
    return NULL;
  return (AdmRegionHeader*)(_adm_start + offset);
}

static inline void*
adm_get_region_base(AdmRegionHeader* region_hdr) {
  if (!region_hdr) return NULL;

  /* validate offset to raw data bytes */
  if (region_hdr->offset > UINTPTR_MAX - _adm_start ||
      region_hdr->offset >= _adm_len) {
    return NULL;
  }
  /* validate data size */
  if (region_hdr->offset + region_hdr->size > UINTPTR_MAX - _adm_start ||
      region_hdr->offset + region_hdr->size > _adm_len) {
    return NULL;
  }

  return (void*)(_adm_start + region_hdr->offset);
}

static inline size_t
adm_region_count() {
  return _adm_hdr ? _adm_hdr->data_n : 0;
}

static inline uintptr_t
adm2ptr(AdmPtr* admptr) {
  if (admptr->offset > _adm_len) return (uintptr_t)NULL;
  return (uintptr_t)(_adm_start + admptr->offset);
}

static inline uintptr_t
ptr2adm(void* ptr) {
  if ((uintptr_t)ptr < _adm_start || (uintptr_t)ptr > (_adm_start + _adm_len))
    return (uintptr_t)NULL;
  return (uintptr_t)((uintptr_t)ptr - _adm_start);
}

#ifdef __cplusplus
}
#endif

#endif
