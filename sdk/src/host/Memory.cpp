//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "Memory.hpp"
#include "./common.h"

#include <sys/stat.h>

namespace Keystone {

Memory::Memory(KeystoneDevice* dev, bool debug)
    : pDevice(dev), logger("sdk", "memory", debug ? Loglevel::TRACE : Loglevel::WARN),
      epmFreeList(0), utmFreeList(0), admFreeList(0), startAddr(0) {}

void Memory::startRuntimeMem() {
    runtimePhysAddr = getCurrentEPMAddress();
    logger.trace("runtime load address = {:x}", runtimePhysAddr);
}

void Memory::startEappMem() {
    eappPhysAddr = getCurrentEPMAddress();
    logger.trace("eapp load address = {:x}", eappPhysAddr);
}

void Memory::startFreeMem() {
    freePhysAddr = getCurrentEPMAddress();
    logger.trace("freemem start address = {:x}", freePhysAddr);
}

void Memory::incrementEPMFreeList() { epmFreeList += PAGE_SIZE; }

uintptr_t Memory::allocPages(size_t size) {
    uintptr_t addr = epmFreeList;
    if (size % PAGE_SIZE > 0) {
        epmFreeList += (size / PAGE_SIZE + 1) * PAGE_SIZE;
    } else {
        epmFreeList += (size / PAGE_SIZE) * PAGE_SIZE;
    }
    logger.trace("allocated epm addr = {:x}", addr);
    return addr;
}

/* This will walk the entire vaddr space in the enclave, validating
   linear at-most-once paddr mappings, and then hashing valid pages */
int Memory::validateAndHashEpm(
    hash_ctx_t* hash_ctx, int level, pte* tb, uintptr_t vaddr, int contiguous,
    uintptr_t* runtime_max_seen, uintptr_t* user_max_seen) {
    return 0;
}

void Memory::init(uintptr_t phys_addr, size_t min_pages) {
    if (!pDevice) {
        logger.warn("KeystoneDevice is not initialized");
        return;
    }
    epmSize     = PAGE_SIZE * min_pages;
    epmFreeList = 0;
    startAddr   = phys_addr;
    logger.trace("private memory initialized, base = {:x}, size = {:x}", startAddr, epmSize);
}

uintptr_t Memory::allocUtm(size_t size) {
    if (!pDevice) {
        logger.warn("KeystoneDevice is not initialized");
        return 0;
    }
    uintptr_t ret = pDevice->initUTM(size);
    utmFreeList   = ret;
    untrustedSize = size;
    utmPhysAddr   = ret;
    logger.trace("shared memory initialized, base = {:x}, size = {:x}", utmPhysAddr, untrustedSize);
    return ret;
}

// TODO: delete this
/* Only used to allocate memory for root page table */
uintptr_t Memory::allocAdm(size_t size, ProtectionTypes protect_type) {
    if (!pDevice) {
        logger.warn("KeystoneDevice is not initialized");
        return 0;
    }
    uintptr_t ret = pDevice->initADM(size, protect_type);
    admFreeList   = ret;
    admSize       = size;
    admPhysAddr   = ret;
    logger.trace("data memory initialized, base = {:x}, size = {:x}", admPhysAddr, admSize);
    return ret;
}

uintptr_t Memory::allocMem(size_t size) {
    if (!pDevice) {
        logger.warn("KeystoneDevice is not initialized");
        return 0;
    }
    uintptr_t ret;
    ret = reinterpret_cast<uintptr_t>(pDevice->map(0, size));
    return ret;
}

// unused
uintptr_t Memory::readMem(uintptr_t src, size_t size) {
    if (!pDevice) {
        logger.warn("KeystoneDevice is not initialized");
        return 0;
    }
    uintptr_t ret;
    ret = reinterpret_cast<uintptr_t>(pDevice->map(src, size));
    return ret;
}

/* src: virtual address */
void Memory::writeMem(uintptr_t src, uintptr_t offset, size_t size) {
    if (!pDevice) {
        logger.warn("KeystoneDevice is not initialized");
        return;
    }
    void* va_dst = pDevice->map(offset, size);
    memcpy(va_dst, reinterpret_cast<void*>(src), size);
}

} // namespace Keystone
