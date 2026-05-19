//******************************************************************************
// Copyright (c) 2020, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "KeystoneDevice.hpp"

#include <sys/mman.h>

namespace Keystone {

KeystoneDevice::KeystoneDevice() {
  eid       = -1;
  pteLocked = false;
}

Error
KeystoneDevice::create(uint64_t minPages) {
  struct keystone_ioctl_create_enclave encl;
  encl.min_pages = minPages;

  if (ioctl(fd, KEYSTONE_IOC_CREATE_ENCLAVE, &encl)) {
    perror("ioctl error");
    eid = -1;
    return Error::IoctlErrorCreate;
  }

  eid      = encl.eid;
  physAddr = encl.pt_ptr;

  return Error::Success;
}

uintptr_t
KeystoneDevice::initUTM(size_t size) {
  struct keystone_ioctl_memalloc encl;
  encl.eid  = eid;
  encl.size = size;
  if (ioctl(fd, KEYSTONE_IOC_UTM_INIT, &encl)) {
    return 0;
  }

  return encl.paddr;
}

uintptr_t
KeystoneDevice::initADM(size_t size, ProtectionTypes protect_type) {
  struct keystone_ioctl_memalloc encl;
  encl.eid          = eid;
  encl.size         = size;
  encl.protect_type = (__u8)protect_type;
  if (ioctl(fd, KEYSTONE_IOC_ADM_INIT, &encl)) {
    return 0;
  }

  return encl.paddr;
}

Error
KeystoneDevice::finalizePte() {
  if (pteLocked) return Error::Success;

  struct keystone_ioctl_memalloc encl;
  encl.eid      = eid;
  encl.mem_type = PROTECTED_MEMORY;
  if (ioctl(fd, KEYSTONE_IOC_SWAP_MAP, &encl)) {
    perror("ioctl error");
    return Error::IoctlErrorFinalizePte;
  }

  pteLocked = true;

  return Error::Success;
}

Error
KeystoneDevice::finalize(
    uintptr_t runtimePhysAddr, uintptr_t eappPhysAddr, uintptr_t freePhysAddr,
    struct runtime_params_t params, std::vector<AdmTypeInfo> type_info) {
  struct keystone_ioctl_create_enclave encl;
  encl.eid           = eid;
  encl.runtime_paddr = runtimePhysAddr;
  encl.user_paddr    = eappPhysAddr;
  encl.free_paddr    = freePhysAddr;
  encl.params        = params;

  size_t type_info_size = type_info.size();
  if (type_info_size > ADM_SLOT_MAX) {
    perror("invalid type info size");
    return Error::IoctlErrorFinalize;
  }
  for (int i = 0; i < type_info_size; i++) {
    encl.type_info[i] = type_info[i];
  }
  for (int i = type_info_size; i < ADM_SLOT_MAX; i++) {
    encl.type_info[i] = (AdmTypeInfo){0, TYPE_NONE, 0};
  }

  if (ioctl(fd, KEYSTONE_IOC_FINALIZE_ENCLAVE, &encl)) {
    perror("ioctl error");
    return Error::IoctlErrorFinalize;
  }
  return Error::Success;
}

Error
KeystoneDevice::destroy() {
  struct keystone_ioctl_create_enclave encl;
  encl.eid = eid;

  /* if the enclave has never created */
  if (eid < 0) {
    return Error::Success;
  }

  if (ioctl(fd, KEYSTONE_IOC_DESTROY_ENCLAVE, &encl)) {
    perror("ioctl error");
    return Error::IoctlErrorDestroy;
  }

  return Error::Success;
}

Error
KeystoneDevice::__run(bool resume, uintptr_t* ret) {
  struct keystone_ioctl_run_enclave encl;
  encl.eid = eid;

  Error error;
  uint64_t request;

  if (resume) {
    error   = Error::IoctlErrorResume;
    request = KEYSTONE_IOC_RESUME_ENCLAVE;
  } else {
    error   = Error::IoctlErrorRun;
    request = KEYSTONE_IOC_RUN_ENCLAVE;
  }

  if (ioctl(fd, request, &encl)) {
    return error;
  }

  switch (encl.error) {
    case KEYSTONE_ENCLAVE_EDGE_CALL_HOST:
      return Error::EdgeCallHost;
    case KEYSTONE_ENCLAVE_INTERRUPTED:
      return Error::EnclaveInterrupted;
    case KEYSTONE_ENCLAVE_DONE:
      if (ret) {
        *ret = encl.value;
      }
      return Error::Success;
    default:
      ERROR(
          "Unknown SBI error (%lu) returned by %s_enclave\n", encl.error,
          resume ? "resume" : "run");
      return error;
  }
}

Error
KeystoneDevice::run(uintptr_t* ret) {
  return __run(false, ret);
}

Error
KeystoneDevice::resume(uintptr_t* ret) {
  return __run(true, ret);
}

void*
KeystoneDevice::map(uintptr_t addr, size_t size) {
  assert(fd >= 0);
  void* ret;
  ret = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr);
  assert(ret != MAP_FAILED);
  return ret;
}

void*
KeystoneDevice::romap(uintptr_t addr, size_t size) {
  assert(fd >= 0);
  void* ret;
  ret = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, addr);
  assert(ret != MAP_FAILED);
  return ret;
}

Error
KeystoneDevice::roprotect(uintptr_t addr, size_t size) {
  int ret;
  ret = mprotect((void*)addr, size, PROT_READ);
  if (ret) {
    return Error::DeviceMemoryMapError;
  }
  return Error::Success;
}

bool
KeystoneDevice::initDevice(Params params) {  // TODO: why does this need params
  /* open device driver */
  fd = open(KEYSTONE_DEV_PATH, O_RDWR);
  if (fd < 0) {
    PERROR("cannot open device file");
    return false;
  }
  return true;
}

Error
MockKeystoneDevice::create(uint64_t minPages) {
  eid = -1;
  return Error::Success;
}

uintptr_t
MockKeystoneDevice::initUTM(size_t size) {
  return 0;
}

uintptr_t
MockKeystoneDevice::initADM(size_t size) {
  return 0;
}

Error
MockKeystoneDevice::finalize(
    uintptr_t runtimePhysAddr, uintptr_t eappPhysAddr, uintptr_t freePhysAddr,
    struct runtime_params_t params) {
  return Error::Success;
}

Error
MockKeystoneDevice::destroy() {
  return Error::Success;
}

Error
MockKeystoneDevice::run(uintptr_t* ret) {
  return Error::Success;
}

Error
MockKeystoneDevice::resume(uintptr_t* ret) {
  return Error::Success;
}

bool
MockKeystoneDevice::initDevice(Params params) {
  return true;
}

void*
MockKeystoneDevice::map(uintptr_t addr, size_t size) {
  sharedBuffer = malloc(size);
  return sharedBuffer;
}

MockKeystoneDevice::~MockKeystoneDevice() {
  if (sharedBuffer) free(sharedBuffer);
}

}  // namespace Keystone
