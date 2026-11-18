/*
 * C2, C3, C5 for the PMP region allocator of sm/src/pmp.c.
 *
 * After the SM and OS regions are set up as in sm_init, K arbitrary
 * operations follow: allocate a region with PMP_PRI_ANY (EPM, ADM) or
 * PMP_PRI_BOTTOM (UTM), or free a region.  Then, for any region i
 * allocated with PMP_PRI_ANY or PMP_PRI_TOP:
 *   C2  its entries lie strictly between the SM entry and the OS entry;
 *   C3  no other region uses its entries, and programmed alone, its entry
 *       matches exactly [addr, addr + size);
 *   C5  a failed allocation changes nothing.
 * The PMP CSRs are modeled as arrays; pmpaddr keeps 54 bits (56-bit
 * physical addresses), and a region ends below 2^56.
 */
#include "pmp.h"

#undef PMP_SET
#undef PMP_UNSET
#define PA_BITS 56
static uintptr_t pmp_addr[PMP_N_REG];
static uint8_t pmp_cfg[PMP_N_REG];
#define PMP_SET(n, g, addr, pmpc)                                                                  \
    {                                                                                              \
        pmp_addr[n] = (addr) & (((uintptr_t)1 << (PA_BITS - 2)) - 1);                              \
        pmp_cfg[n]  = (uint8_t)((pmpc) >> (8 * ((n) % PMP_PER_GROUP)));                            \
    }
#define PMP_UNSET(n, g)                                                                            \
    {                                                                                              \
        pmp_addr[n] = 0;                                                                           \
        pmp_cfg[n]  = 0;                                                                           \
    }

#include "pmp.c"

#ifndef K
#define K 6
#endif

#define A(c, msg) __CPROVER_assert((c), msg)

typedef unsigned __int128 u128;

/* bodies for functions of OpenSBI */
int sbi_printf(const char* fmt, ...) { return 0; }
void spin_lock(spinlock_t* lock) {}
void spin_unlock(spinlock_t* lock) {}

/* the operations performed, for reading counterexamples */
int log_op[K], log_ret[K], log_rid[K];
uintptr_t log_start[K], log_size[K];

void sbi_hart_hang(void) {
    A(0, "sm_assert: the SM hangs");
    __CPROVER_assume(0);
}

/* RISC-V PMP address matching of entry k (OFF, TOR, NAPOT) */
static int entry_matches(int k, u128 x) {
    int a  = (pmp_cfg[k] >> 3) & 3;
    u128 y = pmp_addr[k];
    if (a == 1) {
        u128 lo = k ? (u128)pmp_addr[k - 1] << 2 : 0;
        return lo <= x && x < (y << 2);
    }
    if (a == 3) {
        int t = 0;
        while (t < PA_BITS - 2 && ((y >> t) & 1))
            t++;
        if (t == PA_BITS - 2)
            return 1;
        u128 base = (y & ~(((u128)1 << t) - 1)) << 2;
        return base <= x && x < base + ((u128)1 << (t + 3));
    }
    return 0;
}

/* lowest and highest PMP entry used by region i */
static int entry_lo(int i) {
    return region_needs_two_entries(i) ? region_register_idx(i) - 1 : region_register_idx(i);
}
static int entry_hi(int i) { return region_register_idx(i); }

void harness(void) {
    int prio[PMP_MAX_N_REGION];
    int sm, os, r;

    __CPROVER_assume(pmp_region_init(SMM_BASE, SMM_SIZE, PMP_PRI_TOP, &sm, 0) == 0);
    prio[sm] = PMP_PRI_TOP;
    __CPROVER_assume(pmp_region_init(0, -1UL, PMP_PRI_BOTTOM, &os, 1) == 0);
    prio[os] = PMP_PRI_BOTTOM;

    for (int s = 0; s < K; s++) {
        int op;
        if (op == 0 || op == 1) {
            uintptr_t start, size;
            int pri = op == 0 ? PMP_PRI_ANY : PMP_PRI_BOTTOM;
            __CPROVER_assume(size > 0 && start < ((uintptr_t)1 << PA_BITS));
            __CPROVER_assume(size < ((uintptr_t)1 << PA_BITS) - start);
            uint32_t rb = reg_bitmap, db = region_def_bitmap;
            int ret   = pmp_region_init(start, size, pri, &r, 0);
            log_op[s] = op, log_start[s] = start, log_size[s] = size, log_ret[s] = ret;
            log_rid[s] = r;
            if (ret == 0)
                prio[r] = pri;
            else
                A(reg_bitmap == rb && region_def_bitmap == db,
                  "C5: a failed allocation changes nothing");
        } else {
            __CPROVER_assume(r >= 0 && r < PMP_MAX_N_REGION && is_pmp_region_valid(r));
            __CPROVER_assume(r != sm && r != os);
            pmp_region_free_atomic(r);
            log_op[s] = 2, log_rid[s] = r;
        }
    }

    int i, j;
    __CPROVER_assume(i >= 0 && i < PMP_MAX_N_REGION && is_pmp_region_valid(i));
    __CPROVER_assume(prio[i] != PMP_PRI_BOTTOM);
    __CPROVER_assume(j >= 0 && j < PMP_MAX_N_REGION && is_pmp_region_valid(j) && j != i);

#ifdef REACH
    /* witnesses, expected to fail: the checks below cover both encodings */
    A(!(prio[i] == PMP_PRI_ANY && region_is_tor(i)), "reach: a TOR region of PMP_PRI_ANY");
    A(!(prio[i] == PMP_PRI_ANY && region_is_napot(i)), "reach: a NAPOT region of PMP_PRI_ANY");
#endif
    if (i != sm)
        A(entry_lo(i) > entry_hi(sm) && entry_hi(i) < PMP_N_REG - 1,
          "C2: entries between the SM and OS entries");
    A(entry_hi(i) < entry_lo(j) || entry_hi(j) < entry_lo(i), "C3: no shared PMP entry");

    for (int k = 0; k < PMP_N_REG; k++) {
        pmp_addr[k] = 0;
        pmp_cfg[k]  = 0;
    }
    pmp_set_keystone(i, PMP_R);
    u128 x;
    __CPROVER_assume(x < ((u128)1 << PA_BITS));
    u128 lo = region_get_addr(i), hi = lo + region_get_size(i);
    A(entry_matches(region_register_idx(i), x) == (lo <= x && x < hi),
      "C3: the entry matches exactly the region");
}
