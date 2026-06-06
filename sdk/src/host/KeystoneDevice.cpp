//******************************************************************************
// Copyright (c) 2020, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "KeystoneDevice.hpp"
#include "common.h"
#include "sm_err.h"

#include <sys/mman.h>
#include <system_error>

namespace Keystone {

KeystoneDevice::KeystoneDevice(bool debug)
    : eid(-1), pteLocked(false), fd(::open(KEYSTONE_DEV_PATH, O_RDWR)),
      logger("sdk", "device", debug ? Loglevel::TRACE : Loglevel::WARN) {
    if (!fd) {
        logger.errorErrno("failed to open {}", KEYSTONE_DEV_PATH);
        throw std::system_error(
            errno, std::generic_category(), "failed to open keystone_driver device");
    }
    logger.trace("{} is opened, fd = {}", KEYSTONE_DEV_PATH, fd.get());
}

Error KeystoneDevice::create(uint64_t minPages) {
    assert(fd);
    struct keystone_ioctl_create_enclave encl;
    encl.min_pages = minPages;

    if (::ioctl(fd.get(), KEYSTONE_IOC_CREATE_ENCLAVE, &encl)) {
        logger.errorErrno("ioctl CREATE_ENCLAVE failed");
        eid = -1;
        return Error::IoctlErrorCreate;
    }

    eid      = encl.eid;
    physAddr = encl.pt_ptr;
    logger.trace("enclave is created, id = {}, paddr = {:x}", eid, physAddr);

    return Error::Success;
}

uintptr_t KeystoneDevice::initUTM(size_t size) {
    assert(fd);
    struct keystone_ioctl_memalloc encl;
    encl.eid  = eid;
    encl.size = size;
    if (::ioctl(fd.get(), KEYSTONE_IOC_UTM_INIT, &encl)) {
        logger.errorErrno("ioctl UTM_INIT failed");
        return 0;
    }
    logger.trace("shared memory is allocated, paddr = {:x}", encl.paddr);

    return encl.paddr;
}

uintptr_t KeystoneDevice::initADM(size_t size, ProtectionTypes protect_type) {
    assert(fd);
    struct keystone_ioctl_memalloc encl;
    encl.eid          = eid;
    encl.size         = size;
    encl.protect_type = (__u8)protect_type;
    if (::ioctl(fd.get(), KEYSTONE_IOC_ADM_INIT, &encl)) {
        logger.errorErrno("ioctl ADM_INIT failed");
        return 0;
    }
    logger.trace("data memory is allocated, paddr = {:x}", encl.paddr);

    return encl.paddr;
}

Error KeystoneDevice::finalizePte() {
    assert(fd);
    if (pteLocked)
        return Error::Success;

    struct keystone_ioctl_memalloc encl;
    encl.eid      = eid;
    encl.mem_type = PROTECTED_MEMORY;
    if (::ioctl(fd.get(), KEYSTONE_IOC_SWAP_MAP, &encl)) {
        logger.errorErrno("ioctl SWAP_MAP failed");
        return Error::IoctlErrorFinalizePte;
    }

    pteLocked = true;
    logger.trace("private memory region is finalized");

    return Error::Success;
}

Error KeystoneDevice::finalize(
    uintptr_t runtimePhysAddr, uintptr_t eappPhysAddr, uintptr_t freePhysAddr,
    struct runtime_params_t params, std::vector<AdmTypeInfo> type_info) {
    assert(fd);
    struct keystone_ioctl_create_enclave encl;
    encl.eid           = eid;
    encl.runtime_paddr = runtimePhysAddr;
    encl.user_paddr    = eappPhysAddr;
    encl.free_paddr    = freePhysAddr;
    encl.params        = params;

    size_t type_info_size = type_info.size();
    if (type_info_size > ADM_SLOT_MAX) {
        logger.error("invalid initial information of adm: exceeded maximum slot size");
        return Error::IoctlErrorFinalize;
    }
    for (int i = 0; i < type_info_size; i++) {
        encl.type_info[i] = type_info[i];
    }
    for (int i = type_info_size; i < ADM_SLOT_MAX; i++) {
        encl.type_info[i] = (AdmTypeInfo){0, TYPE_NONE, 0};
    }

    if (::ioctl(fd.get(), KEYSTONE_IOC_FINALIZE_ENCLAVE, &encl)) {
        logger.errorErrno("ioctl FINALIZE_ENCLAVE failed");
        return Error::IoctlErrorFinalize;
    }

    logger.trace("enclave is ready to run, eid = {}", eid);
    return Error::Success;
}

Error KeystoneDevice::destroy() {
    assert(fd);
    struct keystone_ioctl_create_enclave encl;
    encl.eid = eid;

    /* if the enclave has never created */
    if (eid < 0) {
        logger.warn("enclave has never created");
        return Error::Success;
    }

    if (::ioctl(fd.get(), KEYSTONE_IOC_DESTROY_ENCLAVE, &encl)) {
        logger.errorErrno("ioctl DESTROY_ENCLAVE failed");
        return Error::IoctlErrorDestroy;
    }

    logger.trace("enclave is destroyed, eid = {}", eid);
    return Error::Success;
}

Error KeystoneDevice::__run(bool resume, uintptr_t* ret) {
    assert(fd);
    struct keystone_ioctl_run_enclave encl;
    encl.eid = eid;

    Error error;
    uint64_t request;

    if (resume) {
        error   = Error::IoctlErrorResume;
        request = KEYSTONE_IOC_RESUME_ENCLAVE;
        logger.trace("request to resume enclave");
    } else {
        error   = Error::IoctlErrorRun;
        request = KEYSTONE_IOC_RUN_ENCLAVE;
        logger.trace("request to run enclave");
    }

    if (::ioctl(fd.get(), request, &encl)) {
        logger.error("ioctl RUN_ENCLAVE/RESUME_ENCLAVE failed");
        return error;
    }

    switch (encl.error) {
    case KEYSTONE_ENCLAVE_EDGE_CALL_HOST:
        logger.trace("returned from enclave, cause = edgecall");
        return Error::EdgeCallHost;
    case KEYSTONE_ENCLAVE_INTERRUPTED:
        logger.trace("returned from enclave, cause = interrupt");
        return Error::EnclaveInterrupted;
    case KEYSTONE_ENCLAVE_DONE:
        if (ret) {
            *ret = encl.value;
        }
        logger.trace("returned from enclave, cause = exited");
        return Error::Success;
    default:
        logger.error(
            "failed to run/resume enclave, code = {}, err = {}", encl.error,
            sm_error_name((sm_error_t)encl.error));
        return error;
    }
}

Error KeystoneDevice::run(uintptr_t* ret) { return __run(false, ret); }

Error KeystoneDevice::resume(uintptr_t* ret) { return __run(true, ret); }

void* KeystoneDevice::map(uintptr_t addr, size_t size) {
    assert(fd);
    void* ret;
    ret = ::mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd.get(), addr);
    assert(ret != MAP_FAILED);
    logger.trace(
        "mmap succeeded, prot = {}, addr = {:x}, size = {}", PROT_READ | PROT_WRITE, addr, size);
    return ret;
}

void* KeystoneDevice::romap(uintptr_t addr, size_t size) {
    assert(fd);
    void* ret;
    ret = ::mmap(NULL, size, PROT_READ, MAP_SHARED, fd.get(), addr);
    assert(ret != MAP_FAILED);
    logger.trace("mmap succeeded, prot = {}, addr = {:x}, size = {}", PROT_READ, addr, size);
    return ret;
}

Error KeystoneDevice::roprotect(uintptr_t addr, size_t size) {
    int ret;
    ret = ::mprotect((void*)addr, size, PROT_READ);
    if (ret) {
        logger.errorErrno(
            "mprotect failed, addr = {:x}, size = {}, prot = {}", addr, size, PROT_READ);
        return Error::DeviceMemoryMapError;
    }
    logger.trace("mprotect succeeded, addr = {:x}, size = {}, prot = {}", addr, size, PROT_READ);
    return Error::Success;
}

} // namespace Keystone
