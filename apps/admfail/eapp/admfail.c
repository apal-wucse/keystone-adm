/* Shares the ADM for writing once; the host may break it meanwhile. */
#include <stdio.h>

#include "app/syscall.h"
#include "edge/adm.h"

#define OCALL_TOUCH_ADM 1

int main() {
    AdmTypeInfo type_info[ADM_SLOT_MAX];
    size_t size, count;

    uintptr_t adm = map_adm(&size);
    if (adm == (uintptr_t)-1) {
        printf("[eapp] map_adm failed\n");
        return 1;
    }
    adm_init_internals(adm, size);
    printf("[eapp] sharing the ADM for writing\n");

    /* every region, including those the runtime added for printf */
    if (adm_gen_type_info(type_info, &count) != ADM_ERR_SUCCESS) {
        printf("[eapp] adm_gen_type_info failed\n");
        return 1;
    }
    int ret = ocall_share(OCALL_TOUCH_ADM, NULL, 0, (uintptr_t)type_info, count, ADM_SHARE_RW);
    printf("[eapp] resumed, ocall_share = %d\n", ret);
    return 0;
}
