#ifndef __VM_H__
#define __VM_H__

#include "mm/common.h"
#include "mm/mm.h"
#include "mm/vm_defs.h"
#include "util/printf.h"

extern uintptr_t runtime_va_start;

/* root page table */
extern pte* root_page_table;

uintptr_t
kernel_va_to_pa(void* ptr);
uintptr_t
__va(uintptr_t pa);
uintptr_t
__pa(uintptr_t va);

#ifndef LOADER_BIN

extern void* rt_base;
extern uintptr_t kernel_offset;  // TODO: is this needed?
extern uintptr_t load_pa_start;

/* Program break */
extern uintptr_t program_break;

/* freemem */
extern uintptr_t freemem_va_start;
extern size_t freemem_size;

/* shared buffer */
extern uintptr_t shared_buffer;
extern uintptr_t shared_buffer_size;

/* additional memory */
extern uintptr_t additional_memory;
extern uintptr_t additional_memory_size;
extern uintptr_t additional_pa;
extern uintptr_t additional_user_va;

#endif

static inline pte
pte_create(uintptr_t ppn, int type) {
  return (pte)((ppn << PTE_PPN_SHIFT) | PTE_V | (type & PTE_FLAG_MASK));
}

static inline pte
pte_create_invalid(uintptr_t ppn, int type) {
  return (pte)((ppn << PTE_PPN_SHIFT) | (type & PTE_FLAG_MASK & ~PTE_V));
}

static inline pte
ptd_create(uintptr_t ppn) {
  return pte_create(ppn, PTE_V);
}

static inline uintptr_t
ppn(uintptr_t pa) {
  return pa >> RISCV_PAGE_BITS;
}

// this is identical to ppn, but separate it to avoid confusion between va/pa
static inline uintptr_t
vpn(uintptr_t va) {
  return va >> RISCV_PAGE_BITS;
}

static inline uintptr_t
pte_ppn(pte pte) {
  return pte >> PTE_PPN_SHIFT;
}

#ifndef LOADER_BIN
static inline uintptr_t
__adm_offset(uintptr_t va) {
  if (additional_user_va && va >= additional_user_va &&
      va < additional_user_va + additional_memory_size)
    return (uintptr_t)(va - additional_user_va);
  else
    return (uintptr_t)NULL;
}
#endif

#endif
