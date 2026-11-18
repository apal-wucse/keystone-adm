/*
 * The enclave-side library (sdk/src/edge/adm.c) on an ADM the SM accepts
 * (MODEL.typ 6.2): adm_new_region and adm_remove_region write only inside
 * the ADM, keep the other regions and their data where they are, and leave
 * an ADM that the SM accepts with the metadata of adm_gen_type_info.
 * The new region has a non-zero uid and a size that fits its type.
 * The ADM is the first S bytes of a buffer of S_FIX bytes, S <= S_FIX.
 */
#include "adm.h"
#include <stdlib.h>
#include <string.h>

#define A(c, msg) __CPROVER_assert((c), msg)
#define HDR       sizeof(AdmHeader)
#define RHDR      sizeof(AdmRegionHeader)

#ifndef S_FIX
#define S_FIX 400
#endif

/* bound on the number of regions before the operation */
#ifndef N_MAX
#define N_MAX 2
#endif

char* __adm_malloc_start;
char* __adm_malloc_zone_stop;
bool __adm_malloc_initialized;

int sm_validate(uintptr_t adm, uintptr_t size, void* meta);

/* a byte loop: the built-in model is too costly for a symbolic length */
void* memset(void* dst, int c, size_t n) {
    uint8_t* d = dst;
    for (size_t i = 0; i < n; i++)
        d[i] = (uint8_t)c;
    return dst;
}

/* the metadata the runtime passes when it shares the ADM */
static int still_valid(uint8_t* adm, uintptr_t S) {
    AdmTypeInfo ti[ADM_SLOT_MAX];
    size_t count;
    for (int i = 0; i < ADM_SLOT_MAX; i++)
        ti[i] = (AdmTypeInfo){ADM_UID_UNUSED, TYPE_NONE, 0, 0};
    adm_gen_type_info(ti, &count);
    return sm_validate((uintptr_t)adm, S, ti) == 0;
}

static int size_fits(AdmDataTypes t, size_t d) {
    switch (t) {
    case TYPE_BYTE: return d == 1;
    case TYPE_16: return d == 2;
    case TYPE_32: return d == 4;
    case TYPE_64: return d == 8;
    case TYPE_STATIC_ARR_16: return d % 2 == 0;
    case TYPE_STATIC_ARR_32: return d % 4 == 0;
    case TYPE_STATIC_ARR_64: return d % 8 == 0;
    case TYPE_STATIC_ARR_BYTE:
    case TYPE_ARR_BYTE:
    case TYPE_ARR_16:
    case TYPE_ARR_32:
    case TYPE_ARR_64:
    case TYPE_DYN_ALLOC_SPACE: return 1;
    default: return 0;
    }
}

void harness(void) {
    uint8_t adm[S_FIX];
    uintptr_t S, g = 0;
    __CPROVER_assume(S >= HDR && S <= S_FIX);
    AdmHeader* h = (AdmHeader*)adm;

    /* a byte of the buffer after the ADM */
    uint8_t gval = 0;
    if (S < S_FIX) {
        __CPROVER_assume(S <= g && g < S_FIX);
        gval = adm[g];
    }
    AdmTypeInfo meta[ADM_SLOT_MAX];
    __CPROVER_assume(h->data_n <= N_MAX);
    __CPROVER_assume(sm_validate((uintptr_t)adm, S, meta) == 0);
    adm_init_internals((uintptr_t)adm, S);

    /* a byte of some region */
    size_t n0 = h->data_n, k, o, ksize = 0;
    uintptr_t kuid = ADM_UID_UNUSED, kptr = 0;
    uint8_t kval   = 0;
    if (n0 > 0) {
        __CPROVER_assume(k < n0);
        AdmRegionHeader* r = (AdmRegionHeader*)(adm + h->data_offsets[k]);
        __CPROVER_assume(o < r->size);
        kuid  = h->uid_tbl[k];
        kptr  = (uintptr_t)adm + r->offset;
        ksize = r->size;
        kval  = adm[r->offset + o];
    }

    uintptr_t uid, p = 0;
    size_t size;
    AdmDataTypes type;
    _Bool remove;
    ADMERR err     = ADM_ERR_SUCCESS;
    uintptr_t uid0 = n0 > 0 ? h->uid_tbl[0] : ADM_UID_UNUSED;
    if (remove) {
        err = adm_remove_region(uid);
        A(h->data_n == (err == ADM_ERR_SUCCESS ? n0 - 1 : n0), "remove: one region less");
#ifdef REACH
        /* witnesses, expected to fail */
        A(err != ADM_ERR_SUCCESS, "reach: a region is removed");
        A(!(err == ADM_ERR_SUCCESS && n0 >= 2 && uid == uid0),
          "reach: a region before another one is removed");
#endif
    } else {
        __CPROVER_assume(uid != ADM_UID_UNUSED && size_fits(type, size));
        p = (uintptr_t)adm_new_region(size, uid, type);
        if (p) {
            uintptr_t q = 0;
            size_t qsize;
            A(p >= (uintptr_t)adm + HDR + RHDR && size <= S && p - (uintptr_t)adm <= S - size,
              "new: the region is inside the ADM");
            A(adm_get_region(uid, &q, &qsize) == ADM_ERR_SUCCESS && qsize == size &&
                  (size == 0 || q == p),
              "new: the region is found by its uid");
            A(h->data_n == n0 + 1, "new: one region more");
        } else {
            A(h->data_n == n0, "new: no region is added on failure");
        }
#ifdef REACH
        A(!p, "reach: a region is added");
#endif
    }

    A(still_valid(adm, S), "the SM accepts the ADM after the operation");
    if (S < S_FIX)
        A(adm[g] == gval, "nothing is written after the ADM");

    if (kuid != ADM_UID_UNUSED && !(remove && err == ADM_ERR_SUCCESS && uid == kuid)) {
        uintptr_t q = 0;
        size_t qsize;
        A(adm_get_region(kuid, &q, &qsize) == ADM_ERR_SUCCESS && qsize == ksize && q == kptr,
          "the other regions are kept where they are");
        if (q && o < qsize)
            A(*(uint8_t*)(q + o) == kval, "the data of the other regions is kept");
    }
}
