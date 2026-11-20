/*
 * The copy loops of sm/src/mprv.h: for any length up to L_MAX and any
 * alignment of the two addresses, copy_to_sm and copy_from_sm pass the bytes in order and without
 * a gap to the copy functions of mprv.S, stay inside the buffer of the SM and
 * the range of the caller, stop at the first failure, and cover the whole
 * length on success.
 */
#include "mprv.h"

#define A(c, msg) __CPROVER_assert((c), msg)

#ifndef L_MAX
#define L_MAX 80
#endif

static uintptr_t sm0, addr0;
static size_t len, done;
static _Bool failed;
_Bool nondet_bool(void);

static int part(uintptr_t sm, uintptr_t addr, size_t n) {
    A(!failed, "no copy after a failure");
    A(sm == sm0 + done && addr == addr0 + done, "the parts follow each other");
    A(n <= len - done, "a part stays inside the length");
    done += n;
    failed = nondet_bool();
    return failed;
}

int copy1_from_sm(uintptr_t dst, const uint8_t* src) { return part((uintptr_t)src, dst, 1); }
int copy_word_from_sm(uintptr_t dst, const uintptr_t* src) {
    return part((uintptr_t)src, dst, sizeof(uintptr_t));
}
int copy_block_from_sm(uintptr_t dst, const mprv_block* src) {
    return part((uintptr_t)src, dst, sizeof(mprv_block));
}
int copy1_to_sm(uint8_t* dst, uintptr_t src) { return part((uintptr_t)dst, src, 1); }
int copy_word_to_sm(uintptr_t* dst, uintptr_t src) {
    return part((uintptr_t)dst, src, sizeof(uintptr_t));
}
int copy_block_to_sm(mprv_block* dst, uintptr_t src) {
    return part((uintptr_t)dst, src, sizeof(mprv_block));
}

void harness(void) {
    _Bool to_sm;
    uintptr_t s, a;
    size_t l;
    sm0 = s, addr0 = a, len = l;
    /* the loops depend on the addresses only through their alignment */
    __CPROVER_assume(len <= L_MAX && sm0 < REGBYTES && addr0 < REGBYTES);
    int ret = to_sm ? copy_to_sm((void*)sm0, addr0, len) : copy_from_sm(addr0, (void*)sm0, len);
    A(ret ? failed : !failed && done == len, "the whole length is copied on success");
#ifdef REACH
    A(!(ret == 0 && len == L_MAX && sm0 % 8 == 0 && addr0 % 8 == 0), "reach: an aligned copy");
    A(!(ret == 0 && len == L_MAX && sm0 % 8 != 0), "reach: an unaligned copy");
#endif
}
