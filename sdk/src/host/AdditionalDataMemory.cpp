#include "AdditionalDataMemory.hpp"

namespace Keystone {

AdditionalDataMemory::AdditionalDataMemory(bool debug)
    : memReady(false), finalized(false),
      logger("sdk", "adm", debug ? Loglevel::TRACE : Loglevel::WARN) {}

AdditionalDataMemory::~AdditionalDataMemory() {}

Error AdditionalDataMemory::setupMemory(uintptr_t ptr, uintptr_t size, uintptr_t baseAddr) {
    if (!ptr) {
        logger.error("invalid adm pointer address, addr = {:x}", ptr);
        return Error::AdmInvalidMemory;
    }
    if (size == 0) {
        logger.error("invalid adm size, size = {}", size);
        return Error::AdmInvalidMemory;
    }
    if (memReady) {
        logger.warn("adm is already configured, overwriting is not allowed");
        return Error::Success;
    }
    if (finalized) {
        logger.warn("adm is already finalized, overwriting is not allowed");
        return Error::Success;
    }
    adm           = ptr;
    admSize       = size;
    addrInEnclave = baseAddr;
    memReady      = true;
    logger.trace(
        "adm is configured, ptr = {:x}, size = {}, addr(enclave) = {:x}", ptr, size, baseAddr);
    return Error::Success;
}

Error AdditionalDataMemory::setupData(AdditionalData& additionalData) {
    if (finalized) {
        logger.warn("adm is already finalized, overwriting is not allowed");
        return Error::Success;
    }
    this->additionalData = additionalData;
    return Error::Success;
}

Error AdditionalDataMemory::finalize() {
    if (!validateMemory()) {
        logger.error("adm validation failed");
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
        logger.trace(
            "stored data, uid = {}, offset = {:x}, size = {}", d.uid, dataHdr->offset, d.size);

        dataIdx++;
        offset += sizeof(AdmRegionHeader) + (d.size / DATA_ALIGN_BYTES + 1) * DATA_ALIGN_BYTES;
    }
    hdr->free_list = offset;
    finalized      = true;
    return Error::Success;
}

bool AdditionalDataMemory::validateMemory() {
    if (!memReady) {
        logger.warn("validation failed, adm is not ready");
        return false;
    }
    if (additionalData.getStoredSize() > ADM_SLOT_MAX) {
        logger.warn("validation failed, exceed maximum slot size");
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
        logger.warn("validation failed, exceed allocated memory size");
        return false;
    }
    return true;
}

AdmHeader* AdditionalDataMemory::getHdr() {
    if (!finalized) {
        logger.warn("adm is not finalized");
        return nullptr;
    }
    return (AdmHeader*)adm;
}

AdmRegionHeader* AdditionalDataMemory::getRegionHdr(uintptr_t uid) {
    AdmHeader* hdr;
    if (!finalized) {
        logger.warn("adm is not finalized");
        return nullptr;
    }
    hdr = getHdr();
    for (size_t i = 0; i < hdr->data_n; i++) {
        if (hdr->uid_tbl[i] != uid)
            continue;
        logger.trace("found region, uid = {}, offset = {:x}", uid, hdr->data_offsets[i]);
        return (AdmRegionHeader*)(adm + hdr->data_offsets[i]);
    }
    logger.trace("no such region, uid = {}", uid);
    return nullptr;
}

DataVec AdditionalDataMemory::getRegion(uintptr_t uid) {
    AdmRegionHeader* dataHdr;
    DataVec data;
    if (!finalized) {
        logger.warn("adm is not finalized");
        goto nodata;
    }
    dataHdr = getRegionHdr(uid);
    if (dataHdr == nullptr) {
        goto nodata;
    }
    data.assign(
        (uint8_t*)(adm + dataHdr->offset), (uint8_t*)(adm + dataHdr->offset + dataHdr->size));
    logger.trace(
        "found data, uid = {}, offset = {:x}, size = {}", uid, dataHdr->offset, dataHdr->size);
    return data;
nodata:
    return DataVec();
}

} // namespace Keystone
