#ifndef __SM_ERR_H__
#define __SM_ERR_H__

#define SM_ERROR_TABLE                                                                             \
    /* Enclave Error */                                                                            \
    X(SBI_ERR_SM_ENCLAVE_SUCCESS, 0)                                                               \
    X(SBI_ERR_SM_ENCLAVE_UNKNOWN_ERROR, 100000)                                                    \
    X(SBI_ERR_SM_ENCLAVE_INVALID_ID, 100001)                                                       \
    X(SBI_ERR_SM_ENCLAVE_INTERRUPTED, 100002)                                                      \
    X(SBI_ERR_SM_ENCLAVE_PMP_FAILURE, 100003)                                                      \
    X(SBI_ERR_SM_ENCLAVE_NOT_RUNNABLE, 100004)                                                     \
    X(SBI_ERR_SM_ENCLAVE_NOT_DESTROYABLE, 100005)                                                  \
    X(SBI_ERR_SM_ENCLAVE_REGION_OVERLAPS, 100006)                                                  \
    X(SBI_ERR_SM_ENCLAVE_NOT_ACCESSIBLE, 100007)                                                   \
    X(SBI_ERR_SM_ENCLAVE_ILLEGAL_ARGUMENT, 100008)                                                 \
    X(SBI_ERR_SM_ENCLAVE_NOT_RUNNING, 100009)                                                      \
    X(SBI_ERR_SM_ENCLAVE_NOT_RESUMABLE, 100010)                                                    \
    X(SBI_ERR_SM_ENCLAVE_EDGE_CALL_HOST, 100011)                                                   \
    X(SBI_ERR_SM_ENCLAVE_NOT_INITIALIZED, 100012)                                                  \
    X(SBI_ERR_SM_ENCLAVE_NO_FREE_RESOURCE, 100013)                                                 \
    X(SBI_ERR_SM_ENCLAVE_SBI_PROHIBITED, 100014)                                                   \
    X(SBI_ERR_SM_ENCLAVE_ILLEGAL_PTE, 100015)                                                      \
    X(SBI_ERR_SM_ENCLAVE_NOT_FRESH, 100016)                                                        \
    X(SBI_ERR_SM_ENCLAVE_ADM_ILLEGAL_DATA, 100017)                                                 \
    /* PMP Error */                                                                                \
    X(SBI_ERR_SM_PMP_REGION_SIZE_INVALID, 100020)                                                  \
    X(SBI_ERR_SM_PMP_REGION_NOT_PAGE_GRANULARITY, 100021)                                          \
    X(SBI_ERR_SM_PMP_REGION_NOT_ALIGNED, 100022)                                                   \
    X(SBI_ERR_SM_PMP_REGION_MAX_REACHED, 100023)                                                   \
    X(SBI_ERR_SM_PMP_REGION_INVALID, 100024)                                                       \
    X(SBI_ERR_SM_PMP_REGION_OVERLAP, 100025)                                                       \
    X(SBI_ERR_SM_PMP_REGION_IMPOSSIBLE_TOR, 100026)                                                \
    /* Other */                                                                                    \
    X(SBI_ERR_SM_DEPRECATED, 100099)                                                               \
    X(SBI_ERR_SM_NOT_IMPLEMENTED, 100100)

typedef enum {
#define X(name, value) name = value,
    SM_ERROR_TABLE
#undef X
} sm_error_t;

static inline const char* sm_error_name(sm_error_t code) {
    switch ((int)code) {
#define X(name, value)                                                                             \
    case value:                                                                                    \
        return #name;
        SM_ERROR_TABLE
#undef X
    default:
        return "unknown sbi sm error";
    }
}

#endif
