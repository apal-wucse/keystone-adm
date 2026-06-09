// ADM benchmark

#include "edge/adm.h"
#include "edge/edge_call.h"
#include "host/keystone.h"

#include <algorithm>
#include <climits>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

using Bytes = char*;

using random_bytes_engine =
    std::independent_bits_engine<std::default_random_engine, CHAR_BIT, unsigned char>;

int main(int argc, char** argv) {
    Keystone::AdditionalData data;
    ProtectionTypes protection;

    if (argc != 6) {
        std::cerr << "** error: invalid arguments" << std::endl;
        std::cerr << "** usage: " << argv[0]
                  << " <eapp> <runtime> <loader> <protection> <size (mib)>" << std::endl;
        return 1;
    }

    if (atoi(argv[4]) == 1) {
        protection = ProtectionTypes::STRICT;
        std::cout << "[host] ADM Protection Policy: STRICT" << std::endl;
    } else {
        protection = ProtectionTypes::READABLE;
        std::cout << "[host] ADM Protection Policy: READABLE" << std::endl;
    }

    size_t transferSize = atoi(argv[5]) * 1024 * 1024; // transfer argv[5] MiB
    random_bytes_engine rbe;
    std::vector<unsigned char> randBytes(transferSize);
    std::generate(randBytes.begin(), randBytes.end(), std::ref(rbe));
    std::cout << "[host] Generated random bytes: " << transferSize << " MiB" << std::endl;

    uintptr_t data_uid = 1;
    data.storeBytes(randBytes.data(), randBytes.size(), data_uid);

    Keystone::Enclave enclave = Keystone::EnclaveBuilder()
                                    .workingMemory(4 * 1024 * 1024)
                                    .sharedMemory(1024 * 1024)
                                    .dataMemory(PAGE_UP(transferSize + 4096), protection)
                                    .build();

    // enclave initialization
    enclave.init(argv[1], argv[2], argv[3], data);

    // edge call settings
    enclave.registerOcallDispatchProtected(protected_incoming_call_dispatch);
    edge_call_init_internals((uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());
    adm_init_internals((uintptr_t)enclave.getAdditionalMemory(), enclave.getAdditionalMemorySize());

    // run eapp
    uintptr_t transfer_cycles;
    enclave.run(&transfer_cycles);

    // transfer cycles
    std::cout << "PERF-MAGIC-ENC: " << transfer_cycles << std::endl;

    return 0;
}
