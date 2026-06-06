//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "edge/edge_call.h"
#include "host/keystone.h"

using namespace Keystone;

int main(int argc, char** argv) {
    Enclave enclave = EnclaveBuilder().workingMemory(256 * 1024).sharedMemory(256 * 1024).build();
    enclave.init(argv[1], argv[2], argv[3]);
    enclave.registerOcallDispatch(incoming_call_dispatch);
    edge_call_init_internals((uintptr_t)enclave.getSharedBuffer(), enclave.getSharedBufferSize());
    enclave.run();
    return 0;
}
