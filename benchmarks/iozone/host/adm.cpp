#include "edge/adm.h"
#include "edge/edge_call.h"
#include "host/keystone.h"

int main(int argc, char** argv) {
    Keystone::Enclave enclave = Keystone::EnclaveBuilder()
                                    .workingMemory(49152 * 1024)
                                    .sharedMemory(2048 * 1024)
                                    .dataMemory(64 * 1024 * 1024, ProtectionTypes::STRICT)
                                    .build();

    enclave.init(argv[1], argv[2], argv[3]);

    enclave.registerOcallDispatchProtected(protected_incoming_call_dispatch);
    edge_call_init_internals((uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());
    adm_init_internals((uintptr_t)enclave.getAdditionalMemory(), enclave.getAdditionalMemorySize());

    unsigned long ret;
    enclave.run(&ret);

    return ret;
}
