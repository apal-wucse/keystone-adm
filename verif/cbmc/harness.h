/* Common definitions for the harnesses of validate_adm_regions (MODEL.adoc 6). */
#pragma once
#include "adm.h"
#include <stdlib.h>

#define HDR   sizeof(AdmHeader)
#define RHDR  sizeof(AdmRegionHeader)
#define S_MAX ((uintptr_t)1 << 40)

typedef unsigned __int128 u128;

/* An ADM of S bytes with arbitrary contents. */
static inline uint8_t* nondet_adm(uintptr_t S) {
    uint8_t* adm = malloc(S);
    __CPROVER_assume(adm != NULL);
    return adm;
}
