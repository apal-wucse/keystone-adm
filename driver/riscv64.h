#ifndef __RISCV64__
#define __RISCV64__

#include <asm/page.h>
#include <linux/types.h>

#define INSERT_FIELD(val, which, fieldval)                                                         \
    (((val) & ~(which)) | ((fieldval) * ((which) & ~((which) - 1))))

#define PRV_U 0
#define PRV_S 1
#define PRV_H 2
#define PRV_M 3

#define SATP32_MODE 0x80000000
#define SATP32_ASID 0x7FC00000
#define SATP32_PPN  0x003FFFFF
#define SATP64_MODE 0xF000000000000000
#define SATP64_ASID 0x0FFFF00000000000
#define SATP64_PPN  0x00000FFFFFFFFFFF

#define SATP_MODE_OFF  0
#define SATP_MODE_SV32 1
#define SATP_MODE_SV39 8
#define SATP_MODE_SV48 9
#define SATP_MODE_SV57 10
#define SATP_MODE_SV64 11

// page table entry (PTE) fields
#define PTE_V    0x001 // Valid
#define PTE_R    0x002 // Read
#define PTE_W    0x004 // Write
#define PTE_X    0x008 // Execute
#define PTE_U    0x010 // User
#define PTE_G    0x020 // Global
#define PTE_A    0x040 // Accessed
#define PTE_D    0x080 // Dirty
#define PTE_SOFT 0x300 // Reserved for Software

#define PTE_PPN_SHIFT 10

#define PTE_TABLE(PTE) (((PTE) & (PTE_V | PTE_R | PTE_W | PTE_X)) == PTE_V)

#define MSTATUS_SD         MSTATUS64_SD
#define SSTATUS_SD         SSTATUS64_SD
#define RISCV_PGLEVEL_BITS 9

#define RISCV_PGSHIFT 12
#define RISCV_PGSIZE  (1 << RISCV_PGSHIFT)

#define MEGAPAGE_SIZE    ((uintptr_t)(RISCV_PGSIZE << RISCV_PGLEVEL_BITS))
#define SATP_MODE_CHOICE INSERT_FIELD(0, SATP64_MODE, SATP_MODE_SV39)
#define GIGAPAGE_SIZE    (MEGAPAGE_SIZE << RISCV_PGLEVEL_BITS)

/* from runtime/incldue/mm/vm_defs.h */
#define MASK(n) (BIT(n) - 1ul)

#if __riscv_xlen == 64
#define RISCV_PT_INDEX_BITS 9
#define RISCV_PT_LEVELS     3
#elif __riscv_xlen == 32
#define RISCV_PT_INDEX_BITS 10
#define RISCV_PT_LEVELS     2
#endif

#define RISCV_PAGE_BITS         12
#define RISCV_PAGE_SIZE         (1 << RISCV_PAGE_BITS)
#define RISCV_PAGE_OFFSET(addr) (addr % RISCV_PAGE_SIZE)
#define RISCV_GET_PT_INDEX(addr, n)                                                                \
    (((addr) >> (((RISCV_PT_INDEX_BITS) * ((RISCV_PT_LEVELS) - (n))) + RISCV_PAGE_BITS)) &         \
     MASK(RISCV_PT_INDEX_BITS))
#define RISCV_GET_LVL_PGSIZE_BITS(n)                                                               \
    (((RISCV_PT_INDEX_BITS) * (RISCV_PT_LEVELS - (n))) + RISCV_PAGE_BITS)
#define RISCV_GET_LVL_PGSIZE(n) BIT(RISCV_GET_LVL_PGSIZE_BITS((n)))

#define ROUND_UP(n, b)   (((((n) - 1ul) >> (b)) + 1ul) << (b))
#define ROUND_DOWN(n, b) (n & ~((2 << (b - 1)) - 1))
#define PAGE_DOWN(n)     ROUND_DOWN(n, RISCV_PAGE_BITS)
#define PAGE_UP(n)       ROUND_UP(n, RISCV_PAGE_BITS)
#define MEGAPAGE_DOWN(n) ROUND_DOWN(n, RISCV_GET_LVL_PGSIZE_BITS(2))
#define MEGAPAGE_UP(n)   ROUND_UP(n, RISCV_GET_LVL_PGSIZE_BITS(2))

// extern pte_t* root_page_table;

static inline void flush_tlb(void) { asm volatile("sfence.vma"); }

static inline pte_t pte_create(unsigned long ppn, int type) {
    return __pte((ppn << PTE_PPN_SHIFT) | PTE_V | type);
}

static inline pte_t ptd_create(unsigned long ppn) { return pte_create(ppn, PTE_V); }
#endif
