//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#include "Enclave.hpp"

#include <math.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
extern "C" {
#include "./keystone_user.h"
}
#include "ElfFile.hpp"
#include "hash_util.hpp"

namespace Keystone {

Enclave::Enclave(Params params, bool debug)
    : params(params), logger("sdk", "enclave", debug ? Loglevel::TRACE : Loglevel::WARN) {
    pDevice          = std::make_unique<KeystoneDevice>(debug);
    pMemory          = std::make_unique<Memory>(pDevice.get(), debug);
    additionalMemory = std::make_unique<AdditionalDataMemory>(debug);
}

Enclave::~Enclave() {
    if (!pDevice) {
        return;
    }
#ifdef TIME_BENCHMARK
    perfClkStart();
#endif
    Error err = pDevice->destroy();
    if (err != Error::Success) {
        logger.fatal("an error occured when destroying enclave, code = {}", err);
        return;
    }
#ifdef TIME_BENCHMARK
    cycle_destroy_enclave = perfClkEnd();
    perfEventClose();
    timeBenchResult();
#endif
#ifdef CS_BENCHMARK
    cswitchBenchResult();
#endif
}

uint64_t calculate_required_pages(ElfFile** elfFiles, size_t numElfFiles) {
    uint64_t req_pages = 0;

    if (elfFiles == nullptr)
        return 0;

    for (int i = 0; i < numElfFiles; i++) {
        if (elfFiles[i] == nullptr)
            continue;
        ElfFile* elfFile = elfFiles[i];
        req_pages += ceil(elfFile->getFileSize() / PAGE_SIZE);
    }
    /* FIXME: calculate the required number of pages for the page table.
     * We actually don't know how many page tables the enclave might need,
     * because the SDK never knows how its memory will be aligned.
     * Ideally, this should be managed by the driver.
     * For now, we naively allocate enough pages so that we can temporarily get
     * away from this problem.
     * 15 pages will be more than sufficient to cover several hundreds of
     * megabytes of enclave/runtime. */
    /* FIXME Part 2: now that loader does loading, .bss sections also eat up
     * space. Eapp dev must make FREEMEM big enough to fit this!!
     * Possible fix -- re-add exact .bss calculations?
     */

    /* Add one page each for bss segments of runtime and eapp */
    // TODO: add space for stack?
    req_pages += 16;
    return req_pages;
}

bool Enclave::prepareEnclaveMemory(size_t requiredPages, uintptr_t alternatePhysAddr) {
    assert(pDevice);
    assert(pMemory);
    // FIXME: this will be deprecated with complete freemem support.
    // We just add freemem size for now.
    uint64_t minPages;
    minPages = ROUND_UP(params.getFreeMemSize(), PAGE_BITS) / PAGE_SIZE;
    minPages += requiredPages;

    /* Call Enclave Driver */
    if (pDevice->create(minPages) != Error::Success) {
        return false;
    }

    /* We switch out the phys addr as needed */
    uintptr_t physAddr;
    if (alternatePhysAddr) {
        physAddr = alternatePhysAddr;
    } else {
        physAddr = pDevice->getPhysAddr();
    }

    pMemory->init(physAddr, minPages);

    return true;
}

uintptr_t Enclave::copyFile(uintptr_t filePtr, size_t fileSize) {
    assert(pMemory);
    uintptr_t startOffset = pMemory->getCurrentOffset();
    size_t bytesRemaining = fileSize;

    uintptr_t currOffset;
    while (bytesRemaining > 0) {
        currOffset = pMemory->getCurrentOffset();
        pMemory->incrementEPMFreeList();

        size_t bytesToWrite = (bytesRemaining > PAGE_SIZE) ? PAGE_SIZE : bytesRemaining;
        size_t bytesWritten = fileSize - bytesRemaining;

        if (bytesToWrite < PAGE_SIZE) {
            char page[PAGE_SIZE];
            memset(page, 0, PAGE_SIZE);
            memcpy(page, (const void*)(filePtr + bytesWritten), (size_t)(bytesToWrite));
            pMemory->writeMem((uintptr_t)page, currOffset, PAGE_SIZE);
        } else {
            pMemory->writeMem(filePtr + bytesWritten, currOffset, bytesToWrite);
        }
        bytesRemaining -= bytesToWrite;
    }
    return startOffset;
}

static void measureElfFile(hash_ctx_t* hash_ctx, ElfFile* file) {
    assert(hash_ctx != nullptr);
    assert(file != nullptr);
    uintptr_t fptr = (uintptr_t)file->getPtr();
    uintptr_t fend = fptr + (uintptr_t)file->getFileSize();

    for (; fptr < fend; fptr += PAGE_SIZE) {
        if (fend - fptr < PAGE_SIZE) {
            char page[PAGE_SIZE];
            memset(page, 0, PAGE_SIZE);
            memcpy(page, (const void*)fptr, (size_t)(fend - fptr));
            hash_extend_page(hash_ctx, (void*)page);
        } else {
            hash_extend_page(hash_ctx, (void*)fptr);
        }
    }
}

Error Enclave::measure(
    char* hash, const char* eapppath, const char* runtimepath, const char* loaderpath) {
    assert(
        hash != nullptr || eapppath != nullptr || runtimepath != nullptr || loaderpath != nullptr);

    hash_ctx_t hash_ctx;
    hash_init(&hash_ctx);

    ElfFile* loader  = new ElfFile(loaderpath);
    ElfFile* runtime = new ElfFile(runtimepath);
    ElfFile* eapp    = new ElfFile(eapppath);

    uintptr_t sizes[3] = {
        PAGE_UP(loader->getFileSize()), PAGE_UP(runtime->getFileSize()),
        PAGE_UP(eapp->getFileSize())};
    hash_extend(&hash_ctx, (void*)sizes, sizeof(sizes));

    measureElfFile(&hash_ctx, loader);
    delete loader;
    measureElfFile(&hash_ctx, runtime);
    delete runtime;
    measureElfFile(&hash_ctx, eapp);
    delete eapp;

    hash_finalize(hash, &hash_ctx);

    return Error::Success;
}

Error Enclave::init(
    const char* filepath, const char* runtimepath, const char* loader,
    AdditionalData& additionalData) {
    if (!params.isAdmEnabled()) {
        logger.warn("adm is disabled in parameters");
    }
    assert(additionalMemory);
    initialTypeInfo = additionalData.genTypeInfo();
    additionalMemory->setupData(additionalData);
    return this->init(filepath, runtimepath, loader);
}

Error Enclave::init(const char* eapppath, const char* runtimepath, const char* loaderpath) {
    return this->init(eapppath, runtimepath, loaderpath, (uintptr_t)0);
}

Error Enclave::init(
    const char* eapppath, const char* runtimepath, const char* loaderpath,
    uintptr_t alternatePhysAddr) {
    logger.trace("enclave initialization is started");

    assert(pDevice);
    assert(pMemory);
    assert(additionalMemory);

    if (eapppath == nullptr || runtimepath == nullptr || loaderpath == nullptr) {
        logger.error("invalid parameters");
        return Error::InvalidParameter;
    }

#ifdef CS_BENCHMARK
    cs_cnt = 0;
#endif

#if defined(TIME_BENCHMARK)
    cycle_read_bin        = 0;
    cycle_adm_copy        = 0;
    cycle_bin_copy        = 0;
    cycle_adm_alloc       = 0;
    cycle_epm_alloc       = 0;
    cycle_utm_alloc       = 0;
    cycle_create_enclave  = 0;
    cycle_map_utm         = 0;
    cycle_map_adm         = 0;
    cycle_run_enclave     = 0;
    cycle_destroy_enclave = 0;
#endif

#ifdef TIME_BENCHMARK
    if (!perfEventInit()) {
        logger.errorErrno("failed to init perf event");
        return Error::PerfInitFailure;
    }
    perfClkStart();
#endif

    ElfFile* enclaveFile = new ElfFile(eapppath);
    ElfFile* runtimeFile = new ElfFile(runtimepath);
    ElfFile* loaderFile  = new ElfFile(loaderpath);
    ElfFile* elfFiles[3] = {enclaveFile, runtimeFile, loaderFile};
    logger.trace("binaries are loaded");
    logger.trace("eapp = {}, rt = {}, loader = {}", eapppath, runtimepath, loaderpath);
    size_t requiredPages = calculate_required_pages(elfFiles, 3);
    logger.trace("required pages for private memory = {}", requiredPages);

#ifdef TIME_BENCHMARK
    cycle_read_bin = perfClkEnd();
    perfClkStart();
#endif
    /* Alloc EPM */
    if (!prepareEnclaveMemory(requiredPages, alternatePhysAddr)) {
        logger.error("failed to allocate private memory");
        return Error::DeviceError;
    }
#ifdef TIME_BENCHMARK
    cycle_epm_alloc = perfClkEnd();
#endif
    logger.trace("allocation finished: private memory");
#ifdef TIME_BENCHMARK
    perfClkStart();
#endif
    /* Alloc UTM */
    uintptr_t utm_free, adm_ptr;
    utm_free = pMemory->allocUtm(params.getUntrustedSize());

    if (!utm_free) {
        logger.error("failed to allocate shared memory");
        return Error::DeviceError;
    }
#ifdef TIME_BENCHMARK
    cycle_utm_alloc = perfClkEnd();
#endif
    logger.trace("allocation finished: shared memory");
    /* Alloc ADM if needed */
    if (params.isAdmEnabled()) {
#ifdef TIME_BENCHMARK
        perfClkStart();
#endif
        adm_ptr = pMemory->allocAdm(params.getAdditionalSize(), params.getAdmProtectionType());

        if (!adm_ptr) {
            logger.error("failed to allocate data memory");
            return Error::DeviceError;
        }
#ifdef TIME_BENCHMARK
        cycle_adm_alloc = perfClkEnd();
#endif
        logger.trace("allocation finished: data memory");
    }

#ifdef TIME_BENCHMARK
    perfClkStart();
#endif
    /* Copy loader into beginning of enclave memory */
    copyFile((uintptr_t)loaderFile->getPtr(), loaderFile->getFileSize());
    logger.trace("copy finished: loader binary");

    pMemory->startRuntimeMem();
    runtimeElfAddr = copyFile(
        (uintptr_t)runtimeFile->getPtr(),
        runtimeFile->getFileSize()); // TODO: figure out if we need runtimeELFAddr
    logger.trace("copy finished: runtime binary");

    pMemory->startEappMem();
    enclaveElfAddr = copyFile(
        (uintptr_t)enclaveFile->getPtr(),
        enclaveFile->getFileSize()); // TODO: figure out if we need enclaveElfAddr
    logger.trace("copy finished: eapp binary");

    pMemory->startFreeMem();
#ifdef TIME_BENCHMARK
    cycle_bin_copy = perfClkEnd();
#endif

    struct runtime_params_t runtimeParams;
    runtimeParams.untrusted_ptr  = reinterpret_cast<uintptr_t>(utm_free);
    runtimeParams.untrusted_size = reinterpret_cast<uintptr_t>(params.getUntrustedSize());
    if (params.isAdmEnabled()) {
        runtimeParams.additional_ptr  = reinterpret_cast<uintptr_t>(adm_ptr);
        runtimeParams.additional_size = reinterpret_cast<uintptr_t>(params.getAdditionalSize());
    } else {
        runtimeParams.additional_ptr  = 0;
        runtimeParams.additional_size = 0;
    }

    /* Operations for ADM */
    if (params.isAdmEnabled()) {
#ifdef TIME_BENCHMARK
        perfClkStart();
#endif
        /* finalize enclave pte */
        if (pDevice->finalizePte() != Error::Success) {
            logger.error("failed to finalize private memory");
            return Error::DevicePteFinalizeError;
        }

        /* map adm region to user space
         * after this, user space va pointing to adm is stored in additional_memory
         */
        if (!mapAdditionalMem(params.getAdditionalSize())) {
            logger.error("failed to map data memory to userspace");
            return Error::DeviceMemoryMapError;
        }

        /* Now we can access adm region via ptr: additional_memory */

        /* setup adm */
        Error ret;
        ret = additionalMemory->setupMemory(
            (uintptr_t)additional_memory, (uintptr_t)additional_memory_size,
            params.getAdditionalMem());
        if (ret != Error::Success) {
            logger.error("failed to configure data memory");
            return Error::AdmInvalidMemory;
        }
#ifdef TIME_BENCHMARK
        cycle_map_adm = perfClkEnd();
#endif

#ifdef TIME_BENCHMARK
        perfClkStart();
#endif
        /* locate structured data to adm */
        ret = additionalMemory->finalize();
        if (ret != Error::Success) {
            logger.error("failed to finalize data memory");
            return Error::AdmFinalizeFailure;
        }
#ifdef TIME_BENCHMARK
        cycle_adm_copy = perfClkEnd();
#endif
    }

    /* After finalizing, we cannot access epm,
     * and we have read-only access to adm.
     */
#ifdef TIME_BENCHMARK
    perfClkStart();
#endif
    if (pDevice->finalize(
            pMemory->getRuntimePhysAddr(), pMemory->getEappPhysAddr(), pMemory->getFreePhysAddr(),
            runtimeParams, initialTypeInfo) != Error::Success) {
        logger.error("failed to finalize enclave");
        return Error::DeviceError;
    }
#ifdef TIME_BENCHMARK
    cycle_create_enclave = perfClkEnd();
#endif

#ifdef TIME_BENCHMARK
    perfClkStart();
#endif
    if (!mapUntrusted(params.getUntrustedSize())) {
        logger.error("failed to map shared memory to userspace");
        return Error::DeviceMemoryMapError;
    }
#ifdef TIME_BENCHMARK
    cycle_map_utm = perfClkEnd();
#endif

    logger.trace("finished enclave initialization");

    /* ELF files are no longer needed */
    delete enclaveFile;
    delete runtimeFile;
    delete loaderFile;
    return Error::Success;
}

bool Enclave::mapUntrusted(size_t size) {
    assert(pDevice);
    if (size == 0) {
        return true;
    }
    shared_buffer = pDevice->map(0, size);
    if (shared_buffer == NULL) {
        return false;
    }
    shared_buffer_size = size;
    return true;
}

bool Enclave::mapAdditionalMem(size_t size) {
    assert(pDevice);
    if (size == 0) {
        return true;
    }
    additional_memory = pDevice->map(0, size);
    if (additional_memory == NULL) {
        return false;
    }
    additional_memory_size = size;
    return true;
}

Error Enclave::run(uintptr_t* retval) {
    assert(pDevice);
#ifdef TIME_BENCHMARK
    perfClkStart();
#endif
    logger.trace("try to run enclave");
    Error ret = pDevice->run(retval);
    while (ret == Error::EdgeCallHost || ret == Error::EnclaveInterrupted) {
#ifdef CS_BENCHMARK
        cs_cnt++;
#endif
        /* enclave is stopped in the middle. */
        if (ret == Error::EdgeCallHost) {
            logger.trace("edge call is handled");
            if (params.isAdmEnabled()) { // if adm is enabled, force edge protection
                if (oFuncDispatchProtected != NULL) {
                    oFuncDispatchProtected(getSharedBuffer());
                }
            } else {
                if (oFuncDispatch != NULL) {
                    oFuncDispatch(getSharedBuffer());
                }
            }
        } else {
            logger.trace("enclave is interrupted");
        }
        logger.trace("try to resume enclave");
        ret = pDevice->resume(retval);
    }
#ifdef TIME_BENCHMARK
    cycle_run_enclave = perfClkEnd();
#endif
    if (ret != Error::Success) {
        logger.error("failed to run enclave, code = {}", ret);
        return Error::DeviceError;
    }

    logger.trace("finished enclave execution");
    return Error::Success;
}

void* Enclave::getSharedBuffer() { return shared_buffer; }

size_t Enclave::getSharedBufferSize() { return shared_buffer_size; }

void* Enclave::getAdditionalMemory() { return additional_memory; }

size_t Enclave::getAdditionalMemorySize() { return additional_memory_size; }

Memory& Enclave::getMemory() {
    assert(pMemory);
    return *pMemory;
}

Error Enclave::registerOcallDispatch(OcallFunc func) {
    oFuncDispatch = func;
    return Error::Success;
}

Error Enclave::registerOcallDispatchProtected(OcallFunc func) {
    oFuncDispatchProtected = func;
    return Error::Success;
}

#ifdef CS_BENCHMARK
void Enclave::cswitchBenchResult() {
    // Context Switch
    std::cout << "PERF-MAGIC-CS: ";
    std::cout << cs_cnt << std::endl;
}
#endif

#ifdef TIME_BENCHMARK
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>

inline bool Enclave::perfEventInit() {
    struct perf_event_attr pe;
    pid_t pid = ::getpid();
    ::memset(&pe, 0, sizeof(pe));
    pe.type           = PERF_TYPE_HARDWARE;
    pe.size           = sizeof(pe);
    pe.config         = PERF_COUNT_HW_CPU_CYCLES;
    pe.disabled       = 1;
    pe.exclude_kernel = 0;
    perf_fd           = ::syscall(SYS_perf_event_open, &pe, pid, -1, -1, 0);
    if (perf_fd == -1) {
        std::cerr << "Failed to open perf event." << std::endl;
        return false;
    }
    return true;
}

inline void Enclave::perfEventClose() {
    if (perf_fd >= 0) {
        ::close(perf_fd);
        perf_fd = -1;
    }
}

inline void Enclave::perfClkStart() {
    assert(perf_fd >= 0);
    ::ioctl(perf_fd, PERF_EVENT_IOC_RESET, 0);
    ::ioctl(perf_fd, PERF_EVENT_IOC_ENABLE, 0);
}

inline unsigned long Enclave::perfClkEnd() {
    unsigned long count;
    assert(perf_fd >= 0);
    ::ioctl(perf_fd, PERF_EVENT_IOC_DISABLE, 0);
    ssize_t n = ::read(perf_fd, &count, sizeof(count));
    if (n != (ssize_t)sizeof(count)) {
        logger.fatal("perf read returned {} bytes, expected {}", n, sizeof(count));
        return 0;
    }
    return count;
}
#endif

#if defined(TIME_BENCHMARK)
void Enclave::timeBenchResult() {
    std::cout << "PERF-MAGIC-TIME: ";
    // EPM alloc
    std::cout << cycle_epm_alloc << ", ";
    // UTM alloc
    std::cout << cycle_utm_alloc << ", ";
    // ADM alloc
    std::cout << cycle_adm_alloc << ", ";
    // Read binary
    std::cout << cycle_read_bin << ", ";
    // Copy binary
    std::cout << cycle_bin_copy << ", ";
    // Copy ADM data
    std::cout << cycle_adm_copy << ", ";
    // Create Enclave
    std::cout << cycle_create_enclave << ", ";
    // Map UTM
    std::cout << cycle_map_utm << ", ";
    // Map ADM
    std::cout << cycle_map_adm << ", ";
    // Run enclave
    std::cout << cycle_run_enclave << ", ";
    // Destroy enclave
    std::cout << cycle_destroy_enclave << std::endl;
}
#endif

} // namespace Keystone
