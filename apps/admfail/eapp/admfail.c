/* Shares the ADM for writing once; the host may break it meanwhile. */
#include <stdio.h>
#include <string.h>

#include "app/syscall.h"
#include "edge/adm.h"

#define OCALL_TOUCH_ADM 1
#define UID_SYSCALL     0xff00 /* the region of the runtime for syscalls */
#define UID_TEST        7

/* The SM accepts an identifier of at most 1024 bytes.
 * 0, or: 1 no key, 2 the key is all zero, 4 a longer identifier is accepted.
 * A runtime with verif/qemu/seal-outside.patch asks for the key of a 9-byte
 * identifier at an address outside the EPM: 32 refused, 64 nothing written. */
static int check_sealing_key(void) {
    static struct sealing_key key;
    static char ident[2000] = "admfail";
    int nonzero             = 0;

    int ret = get_sealing_key(&key, sizeof(key), ident, 8) ? 1 : 0;
    for (size_t i = 0; i < sizeof(key.key); i++)
        nonzero |= key.key[i];
    if (!nonzero)
        ret |= 2;
    if (!get_sealing_key(&key, sizeof(key), ident, sizeof(ident)))
        ret |= 4;
    int out = get_sealing_key(&key, sizeof(key), ident, 9);
    if (out)
        ret |= out == 77 ? 64 : 32;
    return ret;
}

/* A region made after the region of the runtime: the next syscall removes
 * a region that is not the last one, and the later ones stay (#16).
 * 0, or: 8 the data is lost or has moved, 16 the case did not occur. */
static int check_remove(void) {
    AdmHeader* hdr = adm_get_header();
    uintptr_t p    = 0;
    size_t size    = 0;
    int ret        = 0;

    printf("[eapp] checking the removal of a region\n");
    fflush(stdout);
    if (!adm_region_exists(UID_SYSCALL))
        ret |= 16;
    unsigned char* x = adm_new_region(64, UID_TEST, TYPE_STATIC_ARR_BYTE);
    if (!x)
        return ret | 8;
    memset(x, 0x5a, 64);

    printf("[eapp] a syscall after the new region\n");
    fflush(stdout);
    if (hdr->data_n < 2 || hdr->uid_tbl[hdr->data_n - 1] != UID_SYSCALL ||
        hdr->uid_tbl[hdr->data_n - 2] != UID_TEST)
        ret |= 16;
    if (adm_get_region(UID_TEST, &p, &size) != ADM_ERR_SUCCESS || size != 64 || p != (uintptr_t)x)
        return ret | 8;
    for (size_t i = 0; i < size; i++)
        if (((unsigned char*)p)[i] != 0x5a)
            ret |= 8;
    return ret;
}

int main() {
    AdmTypeInfo type_info[ADM_SLOT_MAX];
    size_t size, count;

    int checks = check_sealing_key();

    uintptr_t adm = map_adm(&size);
    if (adm == (uintptr_t)-1) {
        printf("[eapp] map_adm failed\n");
        return 1;
    }
    adm_init_internals(adm, size);
    checks |= check_remove();
    printf("[eapp] sharing the ADM for writing\n");

    /* every region, including those the runtime added for printf */
    if (adm_gen_type_info(type_info, &count) != ADM_ERR_SUCCESS) {
        printf("[eapp] adm_gen_type_info failed\n");
        return 1;
    }
    int ret = ocall_share(OCALL_TOUCH_ADM, NULL, 0, (uintptr_t)type_info, count, ADM_SHARE_RW);
    printf("[eapp] resumed, ocall_share = %d\n", ret);
    return checks;
}
