//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#pragma once

#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <functional>
#include <iostream>

#include "./common.h"
extern "C" {
#include "common/sha3.h"
}
#include "AdditionalDataMemory.hpp"
#include "ElfFile.hpp"
#include "Error.hpp"
#include "KeystoneDevice.hpp"
#include "Memory.hpp"
#include "Params.hpp"

namespace Keystone {

typedef std::function<void(void*)> OcallFunc;

class Enclave {
 private:
  Params params;
  uintptr_t runtimeElfAddr;
  uintptr_t enclaveElfAddr;
  Memory* pMemory;
  KeystoneDevice* pDevice;
  void* shared_buffer;
  size_t shared_buffer_size;
  bool admEnabled;
  AdditionalDataMemory* additionalMemory;
  void* additional_memory;
  size_t additional_memory_size;
  OcallFunc oFuncDispatch;
  OcallFunc oFuncDispatchProtected;
#ifdef CS_BENCHMARK
  unsigned long cs_cnt;
#endif

  bool mapUntrusted(size_t size);
  bool mapAdditionalMem(size_t size);
  uintptr_t copyFile(uintptr_t filePtr, size_t fileSize);
  void allocUninitialized(ElfFile* elfFile);
  void loadElf(ElfFile* elfFile);

  bool initFiles(const char*, const char*);
  bool initDevice();
  bool prepareEnclaveMemory(size_t requiredPages, uintptr_t alternatePhysAddr);
  bool initMemory();

#if defined(TIME_BENCHMARK)
  unsigned long cycle_epm_alloc;
  unsigned long cycle_utm_alloc;
  unsigned long cycle_adm_alloc;
  unsigned long cycle_read_bin;
  unsigned long cycle_bin_copy;
  unsigned long cycle_adm_copy;
  unsigned long cycle_create_enclave;
  unsigned long cycle_map_utm;
  unsigned long cycle_map_adm;
  unsigned long cycle_run_enclave;
  unsigned long cycle_destroy_enclave;
#endif

#ifdef TIME_BENCHMARK
  int perf_fd;
  bool perfEventInit();
  void perfClkStart();
  unsigned long perfClkEnd();
  void perfEventClose();
#endif

 public:
  Enclave();
  ~Enclave();
  static Error measure(
      char* hash, const char* eapppath, const char* runtimepath,
      const char* loaderpath);
  void* getSharedBuffer();
  size_t getSharedBufferSize();
  void* getAdditionalMemory();
  size_t getAdditionalMemorySize();
  Memory* getMemory();
  uintptr_t getRuntimeElfAddr() { return runtimeElfAddr; }
  uintptr_t getEnclaveElfAddr() { return enclaveElfAddr; }
  Error registerOcallDispatch(OcallFunc func);
  Error registerOcallDispatchProtected(OcallFunc func);
  Error init(
      const char* filepath, const char* runtime, const char* loaderpath,
      Params parameters);
  Error init(
      const char* filepath, const char* runtime, const char* loader,
      Params parameters, AdditionalData& additionalData);
  Error init(
      const char* eapppath, const char* runtimepath, const char* loaderpath,
      Params _params, uintptr_t alternatePhysAddr);
  Error destroy();
  Error run(uintptr_t* ret = nullptr);

#ifdef CS_BENCHMARK
  void cswitchBenchResult();
#endif
#if defined(TIME_BENCHMARK)
  void timeBenchResult();
#endif
};

uint64_t
calculate_required_pages(ElfFile** elfFiles, size_t numElfFiles);

}  // namespace Keystone
