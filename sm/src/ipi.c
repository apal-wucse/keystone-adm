#include "ipi.h"
#include "pmp.h"
#include <sbi/riscv_barrier.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_fifo.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_ipi.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_scratch.h>

void sbi_pmp_ipi_local_update(struct sbi_tlb_info* __info) {
    struct sbi_pmp_ipi_info* info = (struct sbi_pmp_ipi_info*)__info;
    if (info->type == SBI_PMP_IPI_TYPE_SET) {
        pmp_set_keystone(info->rid, (uint8_t)info->perm);
    } else {
        pmp_unset(info->rid);
    }
}

void send_and_sync_pmp_ipi(int region_idx, int type, uint8_t perm) {
    ulong mask                   = 0;
    ulong source_hart            = current_hartid();
    const struct sbi_domain* dom = sbi_domain_thishart_ptr();
    struct sbi_tlb_info tlb_info;
    u32 h;

    sbi_hsm_hart_interruptible_mask(dom, 0, &mask);

    SBI_TLB_INFO_INIT(&tlb_info, type, 0, region_idx, perm, sbi_pmp_ipi_local_update, source_hart);
    sbi_tlb_request(mask, 0, &tlb_info);

    /* A hart outside the mask may have been START_PENDING with the old value
     * restored: resend to every such hart once it is no longer START_PENDING
     * (sbi_tlb_request skips a hart that is not interruptible). */
    sbi_hartmask_for_each_hart(h, &dom->assigned_harts) {
        if (h == source_hart || (h < BITS_PER_LONG && (mask >> h) & 1UL))
            continue;
        while (sbi_hsm_hart_get_state(dom, h) == SBI_HSM_STATE_START_PENDING)
            cpu_relax();
        sbi_tlb_request(1UL, h, &tlb_info);
    }
}
