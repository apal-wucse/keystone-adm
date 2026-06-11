#include "edge/edge_call.h"
#include "host/keystone.h"

int main(int argc, char** argv) {
    Keystone::Enclave enclave =
        Keystone::EnclaveBuilder().workingMemory(49152 * 1024).sharedMemory(2048 * 1024).build();

    enclave.init(argv[1], argv[2], argv[3]);

    enclave.registerOcallDispatch(incoming_call_dispatch);
    edge_call_init_internals((uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

    enclave.run();

    return 0;
}
