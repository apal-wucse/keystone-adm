#include <stdio.h>
#include <stdlib.h>

#include "app/syscall.h"
#include "edge/adm.h"

void hexdump_buffer_align(const unsigned char* buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02X", buf[i]);
        if (i % 4 == 3) printf(" ");
        if (i % 16 == 15) printf("\n");
    }
    return;
}

int main() {
    printf("#####################\n");
    printf("# ADM Transfer Test #\n");
    printf("#####################\n");
    printf("Step 1: Mapping ADM region to enclave user space...");
    size_t size;
    uintptr_t adm = map_adm(&size);
    if (adm == (uintptr_t)-1) {
        printf("ERROR: map failed\n");
        return 1;
    }
    printf("OK\n");
    printf("Successfully mapped ADM, base: 0x%lx, size: %lu\n", adm, size);

    printf("Step 2: Initializing ADM for user space...");
    adm_init_internals(adm, size);
    printf("OK\n");

    printf("Step 3: Fetching region (uid: 1)...");
    uintptr_t ptr;
    size_t data_sz;
    int ret = adm_get_region(1, &ptr, &data_sz);
    if (ret < 0) {
        printf("ERROR: failed to get ptr\n");
        return 1;
    }
    printf("OK\n");
    printf("Successfully fetched region, base: 0x%lx, size: %lu\n", ptr,
           data_sz);

    printf("\n-----------------------------------\n");
    printf("Dump stored data (first 64 bytes)\n");
    printf("-----------------------------------\n");
    hexdump_buffer_align((unsigned char*)ptr, 64);

    return 0;
}
