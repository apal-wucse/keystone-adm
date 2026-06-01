#include <algorithm>
#include <climits>
#include <functional>
#include <iostream>
#include <random>
#include <vector>

#include "edge/edge_call.h"
#include "host/keystone.h"

#define PAGE_SIZE 4096

using random_bytes_engine =
    std::independent_bits_engine<std::default_random_engine, CHAR_BIT, unsigned char>;

void hexdump_buffer_align(const unsigned char* buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02X", buf[i]);
        if (i % 4 == 3)
            printf(" ");
        if (i % 16 == 15)
            printf("\n");
    }
    return;
}

int main(int argc, char** argv) {
    Keystone::Enclave enclave;
    Keystone::Params params;
    Keystone::AdditionalData data;
    ProtectionTypes protection;
    uint64_t testSize, admSize;

    if (argc < 6) {
        std::cerr << "** error: please specify protection policy and test size" << std::endl;
        std::cerr << "** usage: " << argv[0]
                  << " <App Binary> <Runtime Binary> <Loader Binary> "
                     "<Protection Type> <Test Size (B)>"
                  << std::endl;
        return 1;
    }

    if (atoi(argv[4]) == 1) {
        protection = ProtectionTypes::STRICT;
        std::cout << "[host] ADM Protection Policy: STRICT" << std::endl;
    } else {
        protection = ProtectionTypes::READABLE;
        std::cout << "[host] ADM Protection Policy: READABLE" << std::endl;
    }

    testSize = atoi(argv[5]);
    admSize  = ((testSize + PAGE_SIZE - 1) / PAGE_SIZE + 1) * PAGE_SIZE;

    params.setFreeMemSize(1024 * 1024);
    params.setUntrustedMem(DEFAULT_UNTRUSTED_PTR, 1024 * 1024);
    params.setAdditionalMem(DEFAULT_ADM_PTR, admSize, protection);

    random_bytes_engine rbe;
    std::vector<unsigned char> randBytes(testSize);
    std::generate(randBytes.begin(), randBytes.end(), std::ref(rbe));
    std::cout << "[host] Generated random bytes (first 64 bytes): " << std::endl;
    hexdump_buffer_align(randBytes.data(), 64);
    std::cout << std::endl;

    data.storeBytes(randBytes.data(), randBytes.size(), 1);

    params.setTypeInfo(data.genTypeInfo());

    enclave.init(argv[1], argv[2], argv[3], params, data);

    enclave.registerOcallDispatchProtected(incoming_call_dispatch);
    edge_call_init_internals((uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

    enclave.run();

    return 0;
}
