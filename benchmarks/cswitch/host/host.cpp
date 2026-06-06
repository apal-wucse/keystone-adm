#include <edge/edge_call.h>
#include <host/keystone.h>

#include <iostream>

#ifdef ADM_BUILD
#include <edge/adm.h>
#endif

#define OCALL_SWITCH_BENCH 1

void bench_switch(void* buffer) {
    struct edge_call* edge_call        = (struct edge_call*)buffer;
    edge_call->return_data.call_status = CALL_STATUS_OK;
    return;
}

int main(int argc, char** argv) {
#ifdef ADM_BUILD
    if (argc < 5) {
        std::cerr << "**error: please specify protection policy" << std::endl;
        return 1;
    }

    Keystone::AdditionalData data;
    ProtectionTypes protection;
    if (atoi(argv[4]) == 1)
        protection = ProtectionTypes::STRICT;
    else
        protection = ProtectionTypes::READABLE;

    Keystone::Enclave enclave = Keystone::EnclaveBuilder()
                                    .workingMemory(1024 * 1024)
                                    .sharedMemory(1024 * 1024)
                                    .dataMemory(1024 * 1024, protection)
                                    .build();

    char test_data[] = "Hello, World!";
    data.storeBytes((uint8_t*)test_data, strlen(test_data), 1);

    enclave.init(argv[1], argv[2], argv[3], data);
    enclave.registerOcallDispatchProtected(incoming_call_dispatch);
#else
    Keystone::Enclave enclave =
        Keystone::EnclaveBuilder().workingMemory(1024 * 1024).sharedMemory(1024 * 1024).build();
    enclave.init(argv[1], argv[2], argv[3]);
    enclave.registerOcallDispatch(incoming_call_dispatch);
#endif

    register_call(OCALL_SWITCH_BENCH, bench_switch);
    edge_call_init_internals((uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

#ifdef ADM_BUILD
    adm_init_internals((uintptr_t)enclave.getAdditionalMemory(), enclave.getAdditionalMemorySize());
#endif

    enclave.run();
    return 0;
}
