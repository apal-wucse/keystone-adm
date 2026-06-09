// Ecall benchmark

#include "edge/edge_call.h"
#include "host/keystone.h"

#include <algorithm>
#include <climits>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#define ECALL_TRANS_DATA  0x1
#define ECALL_GET_DATA_SZ 0x2

#define SCRATCHPAD_SIZE (1024 * 1024)            // 1MiB
#define MAX_SIZE        (SCRATCHPAD_SIZE + 4096) // Add a page for metadata

using Bytes = char*;

using random_bytes_engine =
    std::independent_bits_engine<std::default_random_engine, CHAR_BIT, unsigned char>;

Bytes global_data       = NULL;
size_t global_data_size = 0;

void transfer_data(void* buffer) {
    struct edge_call* edge_call = (struct edge_call*)buffer;
    uintptr_t call_args;
    size_t arg_len;
    if (edge_call_args_ptr(edge_call, &call_args, &arg_len) != 0) {
        edge_call->return_data.call_status = CALL_STATUS_BAD_OFFSET;
        return;
    }
    // assumed uint64_t (64bit integer)
    uint64_t offset = *((uint64_t*)call_args);
    if (offset >= global_data_size) {
        // invalid offset
        edge_call->return_data.call_status = CALL_STATUS_BAD_OFFSET;
        return;
    }
    size_t transfer_size = std::min((size_t)SCRATCHPAD_SIZE, global_data_size - offset);
    if (edge_call_setup_wrapped_ret(edge_call, global_data + offset, transfer_size)) {
        edge_call->return_data.call_status = CALL_STATUS_BAD_PTR;
    } else {
        edge_call->return_data.call_status = CALL_STATUS_OK;
    }
    return;
}

void pass_data_sz(void* buffer) {
    struct edge_call* edge_call = (struct edge_call*)buffer;
    uintptr_t call_args;
    size_t arg_len;
    if (edge_call_args_ptr(edge_call, &call_args, &arg_len) != 0) {
        edge_call->return_data.call_status = CALL_STATUS_BAD_OFFSET;
        return;
    }

    uintptr_t data_buf = edge_call_data_ptr();
    memcpy((void*)data_buf, (void*)&global_data_size, sizeof(size_t));

    if (edge_call_setup_ret(edge_call, (void*)data_buf, sizeof(size_t))) {
        edge_call->return_data.call_status = CALL_STATUS_BAD_PTR;
    } else {
        edge_call->return_data.call_status = CALL_STATUS_OK;
    }
    return;
}

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "** error: invalid arguments" << std::endl;
        std::cerr << "** usage: " << argv[0] << " <eapp> <runtime> <loader> <size (mib)>"
                  << std::endl;
        return 1;
    }

    size_t transferSize = atoi(argv[4]) * 1024 * 1024; // transfer argv[4] MiB
    random_bytes_engine rbe;
    std::vector<unsigned char> randBytes(transferSize);
    std::generate(randBytes.begin(), randBytes.end(), std::ref(rbe));
    std::cout << "[host] Generated random bytes: " << transferSize << " MiB" << std::endl;

    global_data      = (Bytes)randBytes.data();
    global_data_size = randBytes.size();

    Keystone::Enclave enclave = Keystone::EnclaveBuilder()
                                    .workingMemory(transferSize + 1024 * 1024 * 4)
                                    .sharedMemory(MAX_SIZE)
                                    .build();

    // enclave initialization
    enclave.init(argv[1], argv[2], argv[3]);

    // edge call settings
    enclave.registerOcallDispatch(incoming_call_dispatch);
    register_call(ECALL_TRANS_DATA, transfer_data);
    register_call(ECALL_GET_DATA_SZ, pass_data_sz);
    edge_call_init_internals((uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

    uintptr_t transfer_cycles;
    enclave.run(&transfer_cycles);

    // transfer cycle
    std::cout << "PERF-MAGIC-ENC: " << transfer_cycles << std::endl;
    return 0;
}
