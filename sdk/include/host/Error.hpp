//******************************************************************************
// Copyright (c) 2020, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------

#pragma once

#include <format>

namespace Keystone {

enum class Error {
    Success = 0,
    InitFailure,
    InvalidParameter,
    FileInitFailure,
    DeviceInitFailure,
    DeviceError,
    IoctlErrorCreate,
    IoctlErrorDestroy,
    IoctlErrorFinalize,
    IoctlErrorRun,
    IoctlErrorResume,
    IoctlErrorUTMInit,
    DeviceMemoryMapError,
    ELFLoadFailure,
    InvalidEnclave,
    VSpaceAllocationFailure,
    PageAllocationFailure,
    EdgeCallHost,
    EnclaveInterrupted,
    IoctlErrorFinalizePte,
    DevicePteFinalizeError,
    AdmInvalidMemory,
    AdmFinalizeFailure,
#ifdef TIME_BENCHMARK
    PerfInitFailure,
#endif
};

} // namespace Keystone

template <> struct std::formatter<Keystone::Error> : std::formatter<int> {
    auto format(Keystone::Error c, std::format_context& ctx) const {
        return std::formatter<int>::format(static_cast<int>(c), ctx);
    }
};
/*

{
  private:
  public:
    KeystoneError(errorType c) { code = c; };

    std::string errorString() {
      switch(code) {
        case (Success):
          return "Success";
          break;
        case (OutOfMemory):
          return "Out of Memory";
          break;
        case (InvalidParameter):
          return "Invalid Parameter";
          break;
        case (EnclaveWriteFail):
          return "Enclave Write Fail";
          break;
        case (EnclaveReadFail):
          return "Enclave Read Fail";
          break;
        case (InvalidEnclave):
          return "Invalid Enclave";
          break;
        default:
          return "Unknown Error";
      }
    }
};
*/
