#include <app/string.h>
#include <app/syscall.h>
#include <stdio.h>

#if defined(ADM_BUILD) || defined(ADM_RW_BUILD)
#include <edge/adm.h>
#endif

#define OCALL_SWITCH_BENCH 1

int main() {
#if defined(ADM_RW_BUILD)
    uintptr_t adm_base;
    size_t adm_size;
    adm_base = map_adm(&adm_size);
    adm_init_internals(adm_base, adm_size);

    AdmTypeInfo type_info[ADM_SLOT_MAX];
    size_t count;
    if (adm_gen_type_info(type_info, &count)) {
        printf("** error: failed to generate type info\n");
        return 1;
    }
#endif
    unsigned long start_cycle, end_cycle;
    __asm__ volatile("rdtime %0" : "=r"(start_cycle));
#if defined(ADM_BUILD)
    ocall_share(OCALL_SWITCH_BENCH, NULL, 0, 0, 0, ADM_SHARE_RONLY);
#elif defined(ADM_RW_BUILD)
    ocall_share(OCALL_SWITCH_BENCH, NULL, 0, (uintptr_t)type_info, 1, ADM_SHARE_RW);
#else
    ocall(OCALL_SWITCH_BENCH, NULL, 0, NULL, 0);
#endif
    __asm__ volatile("rdtime %0" : "=r"(end_cycle));
    printf("PERF-MAGIC-CTXSW: %lu\n", (end_cycle - start_cycle));
    return 0;
}
