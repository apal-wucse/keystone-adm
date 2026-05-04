#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "./common.h"
#include "Error.hpp"
#include "Params.hpp"
#include "adm_types.h"

using DataVec = std::vector<uint8_t>;

namespace Keystone {

class AdditionalData {
 public:
  struct DataBytes {
    size_t size;
    uint8_t* data;
    uintptr_t uid;
  };
  AdditionalData();
  bool storeBytes(uint8_t* data, size_t size, uintptr_t uid);
  bool storeVec(DataVec vec, uintptr_t uid);
  size_t getStoredSize();
  const std::vector<DataBytes>& getStoredData();
  std::vector<AdmTypeInfo> genTypeInfo();

 private:
  std::vector<DataBytes> storedData;
};

}  // namespace Keystone
