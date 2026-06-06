#include "Enclave.hpp"
#include "Params.hpp"
#include "adm_types.h"
#include <cstdint>

namespace Keystone {

class EnclaveBuilder {
public:
    EnclaveBuilder() {}
    EnclaveBuilder(Params params) : params(params) {}
    EnclaveBuilder& workingMemory(uint64_t size);
    EnclaveBuilder& sharedMemory(uint64_t size);
    EnclaveBuilder& dataMemory(uint64_t size, ProtectionTypes type);
    EnclaveBuilder& enableDebug();
    Enclave build();

private:
    Params params;
    bool debug = false;
};

} // namespace Keystone
