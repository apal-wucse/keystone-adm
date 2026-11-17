/*
 * B6: validate_adm_regions returns ADM_VALID => WF(mem, S, meta)
 * (MODEL.adoc 6.2, conditions W1-W9).  Arithmetic is done in 128 bits,
 * so that wrap-around cannot hide a violation.
 */
#include "harness.h"

#define A(c, msg) __CPROVER_assert((c), msg)

/* bound on the number of regions (data_n) */
#ifndef N_MAX
#define N_MAX ADM_SLOT_MAX
#endif

/* Expected metadata as a map: the used uids are distinct. */
static int meta_index(const AdmTypeInfo* meta, uintptr_t uid) {
    for (int k = 0; k < ADM_SLOT_MAX; k++)
        if (meta[k].uid != ADM_UID_UNUSED && meta[k].uid == uid)
            return k;
    return -1;
}

static int size_ok(AdmDataTypes t, u128 d, u128 s) {
    switch (t) {
    case TYPE_BYTE: return d == 1;
    case TYPE_16: return d == 2;
    case TYPE_32: return d == 4;
    case TYPE_64: return d == 8;
    case TYPE_STATIC_ARR_BYTE: return d == s;
    case TYPE_STATIC_ARR_16: return d == s * 2;
    case TYPE_STATIC_ARR_32: return d == s * 4;
    case TYPE_STATIC_ARR_64: return d == s * 8;
    case TYPE_ARR_BYTE:
    case TYPE_ARR_16:
    case TYPE_ARR_32:
    case TYPE_ARR_64: return 1;
    case TYPE_DYN_ALLOC_SPACE: return d == s;
    default: return 0;
    }
}

/* [a, a + l) and [b, b + m) share a byte */
static int overlap(u128 a, u128 l, u128 b, u128 m) {
    return l > 0 && m > 0 && a < b + m && b < a + l;
}

void harness(void) {
    uintptr_t S;
    __CPROVER_assume(S >= HDR && S < S_MAX);
    uint8_t* adm = nondet_adm(S);
    AdmTypeInfo meta[ADM_SLOT_MAX];
    for (int i = 0; i < ADM_SLOT_MAX; i++)
        for (int j = i + 1; j < ADM_SLOT_MAX; j++)
            __CPROVER_assume(meta[i].uid == ADM_UID_UNUSED || meta[i].uid != meta[j].uid);
    __CPROVER_assume(((const AdmHeader*)adm)->data_n <= N_MAX);

    if (validate_adm_regions((uintptr_t)adm, S, meta) != ADM_VALID)
        return;

    const AdmHeader* h = (const AdmHeader*)adm;
    size_t n           = h->data_n;
    A(n <= ADM_SLOT_MAX, "W1: n <= SLOT_MAX");
    if (n > ADM_SLOT_MAX)
        return;
#ifdef REACH
    /* witness, expected to fail: a valid ADM with N_MAX regions exists */
    A(n < N_MAX, "reach: valid ADM with N_MAX regions");
#endif

    /* W2 */
    for (size_t i = 0; i < N_MAX && i < n; i++) {
        A(h->uid_tbl[i] != ADM_UID_UNUSED, "W2: uid is not zero");
        A(meta_index(meta, h->uid_tbl[i]) >= 0, "W2: uid is in meta");
        for (size_t j = 0; j < N_MAX && j < i; j++)
            A(h->uid_tbl[i] != h->uid_tbl[j], "W2: uids are distinct");
    }
    for (int k = 0; k < ADM_SLOT_MAX; k++) {
        if (meta[k].uid == ADM_UID_UNUSED)
            continue;
        int found = 0;
        for (size_t i = 0; i < N_MAX && i < n; i++)
            found |= h->uid_tbl[i] == meta[k].uid;
        A(found, "W2: every region in meta is present");
    }

    /* W3, W5 */
    for (size_t i = 0; i < N_MAX && i < n; i++) {
        A(h->data_offsets[i] >= HDR, "W3: region header after the ADM header");
        A((u128)h->data_offsets[i] + RHDR <= S, "W3: region header inside the ADM");
        if (i + 1 < n)
            A(h->data_offsets[i] < h->data_offsets[i + 1], "W5: increasing header offsets");
    }

    /* region headers (read only where they lie inside the ADM) */
    u128 doff[N_MAX], dsize[N_MAX];
    for (size_t i = 0; i < N_MAX && i < n; i++) {
        if ((u128)h->data_offsets[i] + RHDR > S)
            return;
        const AdmRegionHeader* r = (const AdmRegionHeader*)(adm + h->data_offsets[i]);
        doff[i]                  = r->offset;
        dsize[i]                 = r->size;
        int k                    = meta_index(meta, h->uid_tbl[i]);

        A(doff[i] + dsize[i] <= S, "W4: data inside the ADM");
        if (k >= 0) {
            A(r->type == meta[k].type, "W7: region type matches meta");
            A(doff[i] == meta[k].offset, "W8: data offset matches meta");
            A(size_ok(meta[k].type, dsize[i], meta[k].size), "W9: data size matches meta");
        }
    }

    /* W6 */
    for (size_t i = 0; i < N_MAX && i < n; i++) {
        u128 hi = h->data_offsets[i];
        A(!overlap(0, HDR, hi, RHDR), "W6: region header and ADM header are disjoint");
        A(!overlap(0, HDR, doff[i], dsize[i]), "W6: data and ADM header are disjoint");
        for (size_t j = 0; j < N_MAX && j < n; j++) {
            u128 hj = h->data_offsets[j];
            A(!overlap(doff[i], dsize[i], hj, RHDR), "W6: data and region headers are disjoint");
            if (j < i) {
                A(!overlap(hi, RHDR, hj, RHDR), "W6: region headers are disjoint");
                A(!overlap(doff[i], dsize[i], doff[j], dsize[j]), "W6: data are disjoint");
            }
        }
    }
}
