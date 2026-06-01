#ifndef _ADM_UTILS_H_
#define _ADM_UTILS_H_

#include "adm.h"
#include "mprv.h"
#include "sm.h"

/* Copy type_info from enclave */
static inline unsigned long
copy_adm_type_info(uintptr_t src, unsigned long count, AdmTypeInfo* dst) {
    if (count > ADM_SLOT_MAX)
        return SBI_ERR_SM_ENCLAVE_ILLEGAL_ARGUMENT;

    int illegal = copy_to_sm(dst, src, count * sizeof(AdmTypeInfo));

    if (illegal)
        return SBI_ERR_SM_ENCLAVE_ILLEGAL_ARGUMENT;
    else
        return SBI_ERR_SM_ENCLAVE_SUCCESS;
    return 0;
}

#endif
