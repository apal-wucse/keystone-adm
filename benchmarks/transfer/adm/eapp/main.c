#include <stdio.h>
#include <string.h>

#include "app/eapp_utils.h"
#include "app/syscall.h"
#include "edge/adm.h"

#define ADM_DATA_UID 1

int main(void) {
    unsigned long start_cycle, end_cycle;
    start_cycle = read_cycle();

    /* map adm region to userland */
    size_t size;
    uintptr_t adm_ptr = map_adm(&size);
    if (adm_ptr == (uintptr_t)-1)
        EAPP_RETURN(1);

    /* init adm lib */
    adm_init_internals(adm_ptr, size);

    /* get ptr of data in adm */
    uintptr_t ptr;
    size_t data_sz;
    int ret = adm_get_region(ADM_DATA_UID, &ptr, &data_sz);
    if (ret < 0) {
        printf("failed to get adm data\n");
        EAPP_RETURN(1);
    }
    end_cycle = read_cycle();
    EAPP_RETURN((unsigned long)(end_cycle - start_cycle));
}
