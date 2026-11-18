// Validation failure on resume (MODEL.typ Section 8 #1).
// ADMFAIL_CORRUPT=1: break the ADM header while it is shared; the SM must
// destroy the enclave on resume, and the destroy by the SDK must succeed.
// The eapp also checks the sealing key (#15) and the removal of a region
// (#16), and returns the result.
#include <cstdio>
#include <cstdlib>

#include "edge/edge_call.h"
#include "host/keystone.h"

#define OCALL_TOUCH_ADM 1

static Keystone::Enclave* enclave_ptr;
static bool corrupt;

static void touch_adm(void* buffer) {
    struct edge_call* edge_call = (struct edge_call*)buffer;
    AdmHeader* hdr              = (AdmHeader*)enclave_ptr->getAdditionalMemory();

    printf("[host] ADM shared: data_n = %lu\n", hdr->data_n);
    if (corrupt) {
        hdr->data_n = ADM_SLOT_MAX + 1;
        printf("[host] ADM header broken: data_n = %lu\n", hdr->data_n);
    }
    edge_call->return_data.call_status = CALL_STATUS_OK;
}

int main(int argc, char** argv) {
    const char* env = getenv("ADMFAIL_CORRUPT");
    corrupt         = env && env[0] == '1';

    Keystone::AdditionalData data;
    unsigned char bytes[64] = {0};
    data.storeBytes(bytes, sizeof(bytes), 1);

    {
        Keystone::Enclave enclave = Keystone::EnclaveBuilder()
                                        .workingMemory(256 * 1024)
                                        .sharedMemory(256 * 1024)
                                        .dataMemory(16 * 1024, ProtectionTypes::STRICT)
                                        .build();
        enclave_ptr = &enclave;
        if (enclave.init(argv[1], argv[2], argv[3], data) != Keystone::Error::Success) {
            printf("[host] init failed\n");
            return 1;
        }
        register_protected_call(OCALL_TOUCH_ADM, touch_adm);
        enclave.registerOcallDispatchProtected(protected_incoming_call_dispatch);
        edge_call_init_internals(
            (uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());

        uintptr_t checks    = 0;
        Keystone::Error err = enclave.run(&checks);
        printf("[host] run returned %d\n", (int)err);
        if (err == Keystone::Error::Success)
            printf("[host] eapp checks = %lu\n", checks);
        // the enclave is destroyed here
    }
    printf("[host] done\n");
    return 0;
}
