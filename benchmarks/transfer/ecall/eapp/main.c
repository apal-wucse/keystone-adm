#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/eapp_utils.h"
#include "app/syscall.h"
#include "edge/edge_common.h"

#define ECALL_TRANS_DATA  0x1
#define ECALL_GET_DATA_SZ 0x2

#define SCRATCHPAD_SIZE (1024 * 1024)            // 1MiB
#define MAX_SIZE        (SCRATCHPAD_SIZE + 4096) // Add a page for metadata

#define eapp_print(fmt, ...) printf("[eapp] " fmt, ##__VA_ARGS__)

size_t get_data_size() {
    size_t data_sz;
    ocall(ECALL_GET_DATA_SZ, NULL, 0, &data_sz, sizeof(size_t));
    return data_sz;
}

int receive_data(unsigned char* dst, size_t data_sz) {
    int ret;
    struct edge_data ret_data;
    size_t offset = 0;

    if (dst == NULL) {
        // invalid pointer
        eapp_print("error: invalid destination pointer\n");
        return -1;
    }

    // transfer data by ecall
    while (offset < data_sz) {
        ocall(ECALL_TRANS_DATA, &offset, sizeof(size_t), &ret_data, sizeof(struct edge_data));
        ret = copy_from_shared((void*)(dst + offset), ret_data.offset, ret_data.size);
        if (ret) {
            // copy failed
            eapp_print("error: failed to copy\n");
            return -1;
        }
        offset += ret_data.size;
    }
    return 0;
}

int main() {
    unsigned long __begin, __end;
    __begin = read_cycle();

    size_t data_sz = get_data_size();

    unsigned char* buf = (unsigned char*)malloc(data_sz * sizeof(unsigned char));
    if (!buf) {
        eapp_print("error: malloc failed\n");
        EAPP_RETURN(1);
    }

    int ret = receive_data(buf, data_sz);
    if (ret) {
        eapp_print("error: receive failed\n");
        EAPP_RETURN(1);
    }

    __end = read_cycle();
    EAPP_RETURN((unsigned long)(__end - __begin));
}
