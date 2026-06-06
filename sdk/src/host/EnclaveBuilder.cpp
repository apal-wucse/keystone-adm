#include "EnclaveBuilder.hpp"

namespace Keystone {

EnclaveBuilder& EnclaveBuilder::workingMemory(uint64_t size) {
    params.setFreeMemSize(size);
    return *this;
}

EnclaveBuilder& EnclaveBuilder::sharedMemory(uint64_t size) {
    params.setUntrustedMem(DEFAULT_UNTRUSTED_PTR, size);
    return *this;
}

EnclaveBuilder& EnclaveBuilder::dataMemory(uint64_t size, ProtectionTypes type) {
    params.setAdditionalMem(DEFAULT_ADM_PTR, size, type);
    return *this;
}

EnclaveBuilder& EnclaveBuilder::enableDebug() {
    debug = true;
    return *this;
}

Enclave EnclaveBuilder::build() { return Enclave(params, debug); }

} // namespace Keystone
