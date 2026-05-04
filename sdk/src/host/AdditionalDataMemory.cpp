#include "AdditionalDataMemory.hpp"

namespace Keystone {

AdditionalDataMemory::AdditionalDataMemory() {
  memReady  = false;
  finalized = false;
}

AdditionalDataMemory::~AdditionalDataMemory() {}

Error
AdditionalDataMemory::setupMemory(
    uintptr_t ptr, uintptr_t size, uintptr_t baseAddr) {
  if (!ptr) {
    ERROR("Invalid ADM pointer\n");
    return Error::AdmInvalidMemory;
  }

  if (size == 0) {
    ERROR("Invalid ADM size\n");
    return Error::AdmInvalidMemory;
  }

  if (memReady) {
    ERROR("ADM is already set up\n");
    ERROR("Overwriting is not allowed\n");
    return Error::Success;
  }

  if (finalized) {
    ERROR("ADM is already finalized\n");
    ERROR("Overwriting is not allowed\n");
    return Error::Success;
  }

  adm           = ptr;
  admSize       = size;
  addrInEnclave = baseAddr;
  memReady      = true;

  return Error::Success;
}

Error
AdditionalDataMemory::setupData(AdditionalData& additionalData) {
  if (finalized) {
    ERROR("ADM is already finalized\n");
    ERROR("Overwriting is not allowed\n");
    return Error::Success;
  }

  this->additionalData = additionalData;

  return Error::Success;
}

Error
AdditionalDataMemory::finalize() {
  if (!validateMemory()) {
    ERROR("validation failed\n");
    return Error::AdmFinalizeFailure;
  }

  AdmHeader* hdr = (AdmHeader*)adm;
  hdr->data_n    = additionalData.getStoredSize();
  for (int i = 0; i < ADM_SLOT_MAX; i++) {
    hdr->data_offsets[i] = 0;
    hdr->uid_tbl[i]      = 0;
  }

  uintptr_t offset = sizeof(AdmHeader);
  size_t dataIdx   = 0;
  for (const AdditionalData::DataBytes& d : additionalData.getStoredData()) {
    hdr->data_offsets[dataIdx] = offset;
    hdr->uid_tbl[dataIdx]      = d.uid;
    AdmRegionHeader* dataHdr   = (AdmRegionHeader*)(adm + offset);
    dataHdr->offset            = offset + sizeof(AdmRegionHeader);
    dataHdr->size              = d.size;
    dataHdr->type              = TYPE_STATIC_ARR_BYTE;
    uint8_t* dst               = (uint8_t*)(adm + dataHdr->offset);

    std::memcpy(dst, d.data, d.size);

    dataIdx++;
    offset += sizeof(AdmRegionHeader) +
              (d.size / DATA_ALIGN_BYTES + 1) * DATA_ALIGN_BYTES;
  }

  hdr->free_list = offset;

  finalized = true;

  return Error::Success;
}

bool
AdditionalDataMemory::validateMemory() {
  if (!memReady) {
    return false;
  }

  if (additionalData.getStoredSize() > ADM_SLOT_MAX) {
    return false;
  }

  /* Validate total data size */
  size_t total           = 0;
  size_t hdrSize         = sizeof(AdmHeader);
  size_t dataWrapperSize = sizeof(AdmRegionHeader);
  size_t storedNum       = additionalData.getStoredSize();
  total += hdrSize + dataWrapperSize * storedNum;
  for (const AdditionalData::DataBytes& d : additionalData.getStoredData()) {
    size_t dataSize;
    if (d.size % DATA_ALIGN_BYTES != 0) {
      dataSize = (d.size / DATA_ALIGN_BYTES + 1) * DATA_ALIGN_BYTES;
    } else {
      dataSize = d.size;
    }
    total += dataSize;
  }

  if (total > admSize) {
    return false;
  }

  return true;
}

AdmHeader*
AdditionalDataMemory::getHdr() {
  if (!finalized) {
    return NULL;
  }
  return (AdmHeader*)adm;
}

AdmRegionHeader*
AdditionalDataMemory::getRegionHdr(uintptr_t uid) {
  AdmHeader* hdr;
  if (!finalized) {
    return NULL;
  }
  hdr = getHdr();
  for (size_t i = 0; i < hdr->data_n; i++) {
    if (hdr->uid_tbl[i] != uid) continue;
    return (AdmRegionHeader*)(adm + hdr->data_offsets[i]);
  }
  return NULL;
}

DataVec
AdditionalDataMemory::getRegion(uintptr_t uid) {
  AdmRegionHeader* dataHdr;
  DataVec data;

  if (!finalized) {
    goto nodata;
  }

  dataHdr = getRegionHdr(uid);
  if (dataHdr == NULL) {
    goto nodata;
  }

  data.assign(
      (uint8_t*)(adm + dataHdr->offset),
      (uint8_t*)(adm + dataHdr->offset + dataHdr->size));

  return data;

nodata:
  return DataVec();
}

}  // namespace Keystone
