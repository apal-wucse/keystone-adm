#pragma once

#include "./common.h"
#include "AdditionalData.hpp"
#include "Error.hpp"
#include "Params.hpp"

namespace Keystone {

class AdditionalDataMemory {
  private:
    AdditionalData additionalData;
    uintptr_t adm;
    uintptr_t admSize;
    uintptr_t addrInEnclave;
    bool memReady;
    bool finalized;

    bool validateMemory();

    /* Can use after finalize */
    AdmHeader* getHdr();
    AdmRegionHeader* getRegionHdr(uintptr_t uid);

  public:
    AdditionalDataMemory();
    ~AdditionalDataMemory();
    Error setupMemory(uintptr_t ptr, uintptr_t size, uintptr_t baseAddr);
    Error setupData(AdditionalData& additionalData);
    Error finalize();

    /* Can use after finalize */
    DataVec getRegion(uintptr_t uid);
};
} // namespace Keystone
