/* validate_adm_regions for admlib.c, which is compiled with the SDK header. */
#include "../../sm/src/adm.h"

int sm_validate(uintptr_t adm, uintptr_t size, void* meta) {
    return validate_adm_regions(adm, size, meta);
}
