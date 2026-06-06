#include "AdditionalData.hpp"

namespace Keystone {

AdditionalData::AdditionalData() { storedData = std::vector<DataBytes>(); }

bool AdditionalData::storeBytes(uint8_t* data, size_t size, uintptr_t uid) {
    if (storedData.size() >= ADM_SLOT_MAX) {
        return false;
    }
    if (!data) {
        return false;
    }
    DataBytes wrapped = {size, data, uid};
    storedData.push_back(wrapped);
    return true;
}

bool AdditionalData::storeVec(DataVec vec, uintptr_t uid) {
    if (storedData.size() >= ADM_SLOT_MAX) {
        return false;
    }
    if (vec.empty()) {
        return true;
    }

    size_t size  = vec.size();
    uint8_t* buf = new uint8_t[size];
    std::copy(vec.begin(), vec.end(), buf);

    DataBytes wrapper = {size, buf, uid};
    storedData.push_back(wrapper);
    return true;
}

size_t AdditionalData::getStoredSize() { return storedData.size(); }

const std::vector<AdditionalData::DataBytes>& AdditionalData::getStoredData() { return storedData; }

std::vector<AdmTypeInfo> AdditionalData::genTypeInfo() {
    std::vector<AdmTypeInfo> type_info;
    uintptr_t offset = sizeof(AdmHeader);
    for (DataBytes& d : storedData) {
        type_info.push_back(
            (AdmTypeInfo){d.uid, TYPE_STATIC_ARR_BYTE, d.size, offset + sizeof(AdmRegionHeader)});
        offset += sizeof(AdmRegionHeader) + (d.size / DATA_ALIGN_BYTES + 1) * DATA_ALIGN_BYTES;
    }
    return type_info;
}

} // namespace Keystone
