//******************************************************************************
// Copyright (c) 2020, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#pragma once

#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <vector>

#include "./keystone_user.h"
#include "Error.hpp"
#include "KeystoneLogs.hpp"
#include "UniqueFd.hpp"
#include "adm_types.h"

namespace Keystone {

class KeystoneDevice {
protected:
    int eid;
    uintptr_t physAddr;
    uintptr_t admPhysAddr;
    bool pteLocked;

private:
    UniqueFd fd;
    Error __run(bool resume, uintptr_t* ret);
    Logs logger;

public:
    uintptr_t getPhysAddr() { return physAddr; }
    uintptr_t getAdmPhysAddr() { return admPhysAddr; }
    bool isPteLocked() { return pteLocked; }

    KeystoneDevice() : KeystoneDevice(false) {}
    KeystoneDevice(bool debug);
    Error create(uint64_t minPages);
    uintptr_t initUTM(size_t size);
    uintptr_t initADM(size_t size, ProtectionTypes protect_type);
    Error finalizePte();
    Error finalize(
        uintptr_t runtimePhysAddr, uintptr_t eappPhysAddr, uintptr_t freePhysAddr,
        struct runtime_params_t params, std::vector<AdmTypeInfo> type_info);
    Error destroy();
    Error run(uintptr_t* ret);
    Error resume(uintptr_t* ret);
    void* map(uintptr_t addr, size_t size);
    void* romap(uintptr_t addr, size_t size);
    Error roprotect(uintptr_t addr, size_t size);
};

} // namespace Keystone
