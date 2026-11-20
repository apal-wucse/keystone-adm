/*
 * H8 (MODEL.typ 1.4) for the SBI functions of sm/src/sm-sbi.c: with
 * arbitrary arguments and an arbitrary enclave table, the SM touches memory
 * outside its own objects only
 *   - through the MPRV copy functions (checked by the PMP with the
 *     privilege of the caller),
 *   - inside the EPM of the calling enclave (get_sealing_key), or
 *   - inside a region registered with the PMP allocator (zeroing at
 *     creation and destruction).
 * The EPM of the calling enclave is the only object that stands for
 * physical memory, so any other direct access fails the pointer check.
 * Validation and hashing, which read registered regions, are stubs.
 */
#define PLUGIN_ENABLE_MULTIMEM
#include "mprv.h"
#define copy_from_sm h8_copy_from_sm
#define copy_to_sm   h8_copy_to_sm
#include "adm.h"
#include <sbi/riscv_asm.h>
#undef pr_adm_validation_err
#define pr_adm_validation_err(code) ((void)(code))
#include <sbi/sbi_string.h>
#undef csr_write
#define csr_write(csr, val) ((void)(val))
#define sbi_memset(s, c, n) h8_memset((uintptr_t)(s), (n))
void h8_memset(uintptr_t s, size_t n);
int h8_copy_from_sm(uintptr_t dst, void* src, size_t n);
int h8_copy_to_sm(void* dst, uintptr_t src, size_t n);
#include "enclave.c"
#include "plugins/plugins.c"
#include "sm-sbi.c"

#define A(c, msg) __CPROVER_assert((c), msg)

/* the entry of the enclave table of the call */
#ifndef EID
#define EID 0
#endif

#ifndef EPM_MAX
#define EPM_MAX 2048
#endif

static uint8_t epm[EPM_MAX];
static uintptr_t epm_size;
static int cur;
static uintptr_t reg_addr[PMP_MAX_N_REGION], reg_size[PMP_MAX_N_REGION];
static _Bool derived, copied_in, copied_out, zeroed;

int nondet_int(void);
unsigned long nondet_ulong(void);

static int in_epm(uintptr_t p, size_t n) {
    return p >= (uintptr_t)epm && n <= epm_size && p - (uintptr_t)epm <= epm_size - n;
}

/* inside the region last looked up or registered */
static int hint;
static int in_region(uintptr_t p, size_t n) {
    return p >= reg_addr[hint] && n <= reg_size[hint] && p - reg_addr[hint] <= reg_size[hint] - n;
}

static int in_sm(const void* p, size_t n) {
    return __CPROVER_w_ok(p, n) && !__CPROVER_same_object(p, epm);
}

/* the MPRV copy: the address of the caller is not dereferenced (the loops
 * of mprv.h over the copy functions are checked in mprv.c) */
int h8_copy_from_sm(uintptr_t dst, void* src, size_t n) {
    if (n)
        A(__CPROVER_r_ok(src, n) && !__CPROVER_same_object(src, epm), "copy: read inside the SM");
    copied_out = 1;
    return nondet_int();
}
int h8_copy_to_sm(void* dst, uintptr_t src, size_t n) {
    if (n) {
        A(in_sm(dst, n), "copy: written inside the SM");
        __CPROVER_havoc_object(dst);
    }
    copied_in = 1;
    return nondet_int();
}
int copy_word_from_sm(uintptr_t dst, const uintptr_t* src) {
    (void)*src;
    copied_out = 1;
    return nondet_int();
}

/* only registered regions are zeroed */
void h8_memset(uintptr_t s, size_t n) {
    if (n) {
        A(in_region(s, n), "memset: inside a registered region");
        zeroed = 1;
    }
}

void* sbi_memcpy(void* d, const void* s, size_t n) {
    if (n) {
        A(in_sm(d, n) && in_sm(s, n), "memcpy: inside the SM");
        __CPROVER_havoc_object(d);
    }
    return d;
}

int sm_derive_sealing_key(
    unsigned char* key, const unsigned char* ident, size_t n, const unsigned char* hash) {
    A(in_epm((uintptr_t)key, SEALING_KEY_SIZE), "seal: the key is written inside the EPM");
    A(in_epm((uintptr_t)ident, n), "seal: the identifier is read inside the EPM");
    A(n <= SEALING_KEY_IDENT_MAXLEN, "seal: the identifier is bounded");
    A(in_sm(hash, MDSIZE), "seal: the hash is in the SM");
    derived = 1;
    return nondet_int();
}

void sm_sign(void* sig, const void* data, size_t n) {
    A(in_epm((uintptr_t)sig, SIGNATURE_SIZE) || in_sm(sig, SIGNATURE_SIZE),
      "sign: the signature is written inside the EPM or the SM");
    A(in_epm((uintptr_t)data, n) || in_sm(data, n), "sign: the data is inside the EPM or the SM");
}

uintptr_t pmp_region_get_addr(region_id i) {
    if (i < 0 || i >= PMP_MAX_N_REGION)
        return 0;
    hint = i;
    return reg_addr[i];
}
uint64_t pmp_region_get_size(region_id i) {
    return 0 <= i && i < PMP_MAX_N_REGION ? reg_size[i] : 0;
}
int pmp_region_init_atomic(
    uintptr_t start, uint64_t size, enum pmp_priority pri, region_id* rid, int allow_overlap) {
    int r = nondet_int();
    if (r < 0 || r >= PMP_MAX_N_REGION)
        return -1;
    reg_addr[r] = start;
    reg_size[r] = size;
    *rid        = r;
    hint        = r;
    return 0;
}

/* the rest of the SM: no access to memory outside the SM */
int pmp_region_free_atomic(region_id r) { return nondet_int(); }
int pmp_set_keystone(region_id n, uint8_t perm) { return nondet_int(); }
int pmp_set_global(region_id n, uint8_t perm) { return nondet_int(); }
int pmp_unset_global(region_id n) { return nondet_int(); }
int osm_pmp_set(uint8_t perm) { return nondet_int(); }
void swap_prev_state(struct thread_state* t, struct sbi_trap_regs* regs, int r) {
    struct sbi_trap_regs v;
    struct thread_state w;
    *regs = v;
    *t    = w;
}
void swap_prev_mepc(struct thread_state* t, struct sbi_trap_regs* regs, uintptr_t mepc) {
    regs->mepc = nondet_ulong();
}
void swap_prev_mstatus(struct thread_state* t, struct sbi_trap_regs* regs, uintptr_t mstatus) {
    regs->mstatus = nondet_ulong();
}
void clean_state(struct thread_state* t) {}
void switch_vector_enclave() {}
void switch_vector_host() {}
void cpu_enter_enclave_context(enclave_id eid) {}
void cpu_exit_enclave_context() {}
void platform_init_enclave(struct enclave* e) {}
unsigned long platform_create_enclave(struct enclave* e) { return nondet_ulong(); }
void platform_destroy_enclave(struct enclave* e) {}
void platform_switch_to_enclave(struct enclave* e) {}
void platform_switch_from_enclave(struct enclave* e) {}
uint64_t platform_random() { return nondet_ulong(); }
unsigned long validate_and_hash_enclave(struct enclave* e) { return nondet_ulong(); }
AdmValidationError validate_adm_regions(uintptr_t adm, uintptr_t size, AdmTypeInfo* meta) {
    AdmValidationError v;
    return v;
}

int cpu_get_enclave_id() { return cur; }
void sbi_trap_exit(const struct sbi_trap_regs* regs) {}
void spin_lock(spinlock_t* lock) {}
void spin_unlock(spinlock_t* lock) {}
int sbi_printf(const char* fmt, ...) { return 0; }

void harness(void) {
    struct sbi_trap_regs regs;
    unsigned long a0, a1, a2, a3, out, ret = 1;
    unsigned fid;

    for (int i = 0; i < ENCL_MAX; i++) {
        struct enclave e;
        enclaves[i] = e;
    }
    __CPROVER_havoc_object(reg_addr);
    __CPROVER_havoc_object(reg_size);
    epm_size = nondet_ulong();
    __CPROVER_assume(epm_size <= EPM_MAX);

    /* One run per entry of the table: the call is about entry EID (create:
     * the only free entry).  With EID >= ENCL_MAX, the host passes an id
     * outside the table, and no entry is free. */
    cur = EID < ENCL_MAX ? EID : 0;
    if (EID >= ENCL_MAX)
        __CPROVER_assume(fid <= FID_RANGE_HOST);

    /* called from an enclave: its EPM region, if any, is the object epm */
    _Bool enclave = fid > FID_RANGE_HOST;
    if (enclave) {
        int m = get_enclave_region_index(cur, REGION_EPM);
        if (m >= 0) {
            region_id r = enclaves[cur].regions[m].pmp_rid;
            __CPROVER_assume(0 <= r && r < PMP_MAX_N_REGION);
            __CPROVER_assume(reg_addr[r] == (uintptr_t)epm && reg_size[r] == epm_size);
        }
    }

#ifdef ONLY
    __CPROVER_assume(fid == ONLY);
#endif
    switch (fid) {
    case SBI_SM_CREATE_ENCLAVE:
        for (int i = 0; i < ENCL_MAX; i++)
            enclaves[i].state = i == EID ? INVALID : FRESH;
        ret = sbi_sm_create_enclave(&out, a0);
        break;
    case SBI_SM_DESTROY_ENCLAVE: ret = sbi_sm_destroy_enclave((unsigned long)EID); break;
    case SBI_SM_RUN_ENCLAVE: sbi_sm_run_enclave(&regs, (unsigned long)EID); break;
    case SBI_SM_RESUME_ENCLAVE: sbi_sm_resume_enclave(&regs, (unsigned long)EID); break;
    case SBI_SM_RANDOM: out = sbi_sm_random(); break;
    case SBI_SM_ATTEST_ENCLAVE: ret = sbi_sm_attest_enclave(a0, a1, a2); break;
    case SBI_SM_GET_SEALING_KEY: ret = sbi_sm_get_sealing_key(a0, a1, a2); break;
    case SBI_SM_STOP_ENCLAVE: sbi_sm_stop_enclave(&regs, a0); break;
    case SBI_SM_EXIT_ENCLAVE: sbi_sm_exit_enclave(&regs, a0); break;
    case SBI_SM_STOP_ENCLAVE_SHARE: sbi_sm_stop_enclave_with_share(&regs, a0, a1, a2); break;
    case SBI_SM_CALL_PLUGIN: ret = sbi_sm_call_plugin(a0, a1, a2, a3); break;
    default: break;
    }

#ifdef REACH
    /* witnesses, expected to fail */
    A(!(fid == SBI_SM_GET_SEALING_KEY && ret == 0 && derived), "reach: a sealing key is derived");
    A(!(fid == SBI_SM_ATTEST_ENCLAVE && ret == 0 && copied_in && copied_out),
      "reach: a report is copied out");
    A(!(fid == SBI_SM_CREATE_ENCLAVE && ret == 0 && zeroed), "reach: an enclave is created");
    A(!(fid == SBI_SM_DESTROY_ENCLAVE && ret == 0 && zeroed), "reach: an enclave is destroyed");
    A(!(fid == SBI_SM_STOP_ENCLAVE_SHARE && copied_in &&
        regs.a0 == SBI_ERR_SM_ENCLAVE_EDGE_CALL_HOST),
      "reach: an enclave stops with metadata");
    A(!(fid == SBI_SM_CALL_PLUGIN && copied_out), "reach: a plugin copies a word out");
#endif
}
