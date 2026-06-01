//******************************************************************************
// Copyright (c) 2018, The Regents of the University of California (Regents).
// All Rights Reserved. See LICENSE for license details.
//------------------------------------------------------------------------------
#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include "edge_syscall.h"
#include "mm/vm.h"
#include "util/printf.h"
#include "util/regs.h"

#define RUNTIME_SYSCALL_UNKNOWN         1000
#define RUNTIME_SYSCALL_OCALL           1001
#define RUNTIME_SYSCALL_SHAREDCOPY      1002
#define RUNTIME_SYSCALL_ATTEST_ENCLAVE  1003
#define RUNTIME_SYSCALL_GET_SEALING_KEY 1004
#define RUNTIME_SYSCALL_MAP_ADDITIONAL  1008
#define RUNTIME_SYSCALL_OCALL_SHARE     1009
#define RUNTIME_SYSCALL_READ_CYCLE      1010 // for performance measurement
#define RUNTIME_SYSCALL_EXIT            1101

#define SYSCALL_ADMPTR_BUFFER 0xf000

void handle_syscall(struct encl_ctx* ctx);
void init_edge_internals(void);
uintptr_t dispatch_edgecall_syscall(struct edge_syscall* syscall_data_ptr, size_t data_len);
uintptr_t dispatch_edgecall_syscall_protected(
    struct edge_syscall* syscall_data_ptr, size_t data_len, bool allow_write);
void init_adm_internals(uintptr_t pa);

#ifdef USE_EDGE_PROTECTION
static inline uintptr_t
setup_adm_syscall_metadata(size_t call_num, size_t argsize, size_t bufsize) {
    uintptr_t base;
    // try remove existing metadata region
    ADMERR err = adm_remove_region(ADM_UID_SYSCALL_META);
    if (err == ADM_ERR_NOT_AVAILABLE)
        return (uintptr_t)NULL;
    // create metadata region
    // info + args + buffer + retval (uintptr_t)
    base = (uintptr_t)adm_new_region(
        sizeof(struct edge_syscall) + argsize + bufsize + sizeof(uintptr_t), ADM_UID_SYSCALL_META,
        TYPE_STATIC_ARR_BYTE);
    if (!base)
        return (uintptr_t)NULL;
    ((struct edge_syscall*)base)->syscall_num = call_num;
    return base;
}
#endif

// Define this to enable printing of a large amount of syscall information
// #define USE_INTERNAL_STRACE 1

#ifdef USE_INTERNAL_STRACE
#define print_strace printf
#else
#define print_strace(...)
#endif

#endif /* syscall.h */
