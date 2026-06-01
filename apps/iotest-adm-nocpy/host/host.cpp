//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include <iostream>

#include "edge/adm.h"
#include "edge/edge_call.h"
#include "host/keystone.h"

using namespace Keystone;

int main(int argc, char** argv) {
    Enclave enclave;
    Params params;
    ProtectionTypes protection;

    if (argc < 5) {
        std::cerr << "** error: please specify protection policy" << std::endl;
        return 1;
    }

    if (atoi(argv[4]) == 1) {
        protection = ProtectionTypes::STRICT;
        std::cout << "[host] ADM Protection Policy: STRICT" << std::endl;
    } else {
        protection = ProtectionTypes::READABLE;
        std::cout << "[host] ADM Protection Policy: READABLE" << std::endl;
    }

    params.setFreeMemSize(256 * 1024);
    params.setUntrustedMem(DEFAULT_UNTRUSTED_PTR, 2048 * 1024);
    params.setAdditionalMem(DEFAULT_ADM_PTR, 4 * 1024 * 1024, protection);

    enclave.init(argv[1], argv[2], argv[3], params);

    enclave.registerOcallDispatchProtected(protected_incoming_call_dispatch);
    edge_call_init_internals((uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());
    adm_init_internals((uintptr_t)enclave.getAdditionalMemory(), enclave.getAdditionalMemorySize());

    enclave.run();

    return 0;
}
