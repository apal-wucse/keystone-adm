/* Stops with the ADM shared for writing, then without a share, forever. */
#include "app/syscall.h"
#include "edge/adm.h"

#define OCALL_SHARED   1
#define OCALL_UNSHARED 2
#define UID_SPIN       1 /* the length of the computation, set by the host */

int main() {
    AdmTypeInfo type_info[ADM_SLOT_MAX];
    size_t size, count;
    uintptr_t p = 0;
    size_t len  = 0;

    uintptr_t adm = map_adm(&size);
    if (adm == (uintptr_t)-1)
        return 1;
    adm_init_internals(adm, size);
    if (adm_get_region(UID_SPIN, &p, &len) != ADM_ERR_SUCCESS || len != sizeof(unsigned long))
        return 2;
    unsigned long spin = *(unsigned long*)p;
    if (adm_gen_type_info(type_info, &count) != ADM_ERR_SUCCESS)
        return 3;

    for (;;) {
        ocall_share(OCALL_SHARED, NULL, 0, (uintptr_t)type_info, count, ADM_SHARE_RW);
        for (volatile unsigned long i = 0; i < spin; i++)
            ;
        ocall_share(OCALL_UNSHARED, NULL, 0, 0, 0, ADM_SHARE_NONE);
        for (volatile unsigned long i = 0; i < spin; i++)
            ;
    }
}
