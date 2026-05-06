#define __PREPROCESSING__
#include "mm/vm_defs.h"
#include <asm/csr.h>
#include "util/asm_helpers.h"

.section .text
_start:
  // a1: dram_base
  // a2: dram_size
  // a3: runtime_base
  // a4: user_base
  // a5: free_base
  // a6: untrusted_ptr
  // a7: untrusted_size
  // t1: additional_ptr
  // t2: additional_size

  // use designated stack
  la sp, _estack

  // save all args to stack 
  addi sp, sp, -(REGBYTES*9)
  STORE a1, 0(sp)
  STORE a2, 1*REGBYTES(sp)
  STORE a3, 2*REGBYTES(sp)
  STORE a4, 3*REGBYTES(sp)
  STORE a5, 4*REGBYTES(sp)
  STORE a6, 5*REGBYTES(sp)
  STORE a7, 6*REGBYTES(sp)
  STORE t1, 7*REGBYTES(sp)
  STORE t2, 8*REGBYTES(sp)

  // store args in t1,t2 to stack following C ABI
  addi sp, sp, -(REGBYTES*2)
  STORE t1, 0(sp)
  STORE t2, 1*REGBYTES(sp)

  // call load_runtime
  call load_runtime 

  addi sp, sp, REGBYTES*2

  // exit if errors
  bne a0, zero, exit

  // switch to va and jump to runtime code 
  li t0, RUNTIME_VA_START
  csrw stvec, t0  // store runtime start addresss in stvec 

  // construct new satp
  // below assembly and fences work on CVA6, for satp specifically.
  // FIXME: declutter if possible according to more testing
  la a0, root_page_table_storage  
  li a1, RISCV_PAGE_BITS
  li a2, SATP_MODE
  srl a0, a0, a1
  or a0, a0, a2

  // flush TLB's just in case
  fence.i
  sfence.vma

  // set arguments for eyrie_boot
  LOAD a1, 0(sp)
  LOAD a2, 1*REGBYTES(sp)
  LOAD a3, 2*REGBYTES(sp)
  LOAD a4, 3*REGBYTES(sp)
  LOAD a5, free_base_final
  LOAD a6, 5*REGBYTES(sp)
  LOAD a7, 6*REGBYTES(sp)
  LOAD t1, 7*REGBYTES(sp)
  LOAD t2, 8*REGBYTES(sp)

  // flush TLB's just in case
  fence.i
  sfence.vma

  csrw satp, a0 // switch to virtual addresssing 
  sfence.vma

exit:
  call error_and_exit
