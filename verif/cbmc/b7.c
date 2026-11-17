/*
 * B7: for any ADM contents, any metadata and any size S > 0,
 * validate_adm_regions reads only [0, S), does not overflow and terminates.
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

    validate_adm_regions((uintptr_t)adm, S, meta);
}
