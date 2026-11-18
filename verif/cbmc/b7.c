/*
 * B7: for any ADM contents, any metadata and any size S > 0,
 * validate_adm_regions reads only [0, S), writes nothing to the ADM, does
 * not overflow and terminates.
 * With -DHEADER_FITS, S >= sizeof(AdmHeader) is assumed (an ADM is at
 * least one page).
 */
#include "harness.h"

void harness(void) {
    uintptr_t S;
    __CPROVER_assume(S > 0 && S < S_MAX);
#ifdef HEADER_FITS
    __CPROVER_assume(S >= HDR);
#endif
    uint8_t* adm = nondet_adm(S);
    AdmTypeInfo meta[ADM_SLOT_MAX];

    uintptr_t k;
    __CPROVER_assume(k < S);
    uint8_t before = adm[k];

    validate_adm_regions((uintptr_t)adm, S, meta);

    __CPROVER_assert(adm[k] == before, "B7: the ADM is not written");
}
