// Stop and resume in a race (MODEL.typ Section 8 #13).
// The eapp stops with the ADM shared for writing, then without a share, in
// a loop. Two threads on CPU 1 and 2 resume the enclave. In each round one
// of them runs the enclave up to the stop with the share while the other
// keeps issuing resume, so that a resume arrives right when the other hart
// stops the enclave. The thread whose resume was accepted then runs the
// enclave up to the stop without a share, alone, and reads the ADM from
// every CPU: with the DEFAULT_NOPERM policy no CPU may have access.
// ADMRACE_TRIALS (default 2000), ADMRACE_SPIN (default 1000). Run as root.
#include <atomic>
#include <csetjmp>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>

#include "edge/edge_call.h"
#include "host/keystone.h"
#include "host/sm_err.h"

#define NCPU           4
#define OCALL_SHARED   1
#define OCALL_UNSHARED 2

static int dev;
static uintptr_t eid;
static volatile unsigned char* adm;
static volatile struct edge_call* call;
static sigjmp_buf fault_env;
static std::atomic<int> done, failed;
static std::atomic<unsigned long> round_no, entered;
static std::atomic<unsigned long> n_busy, checked, skipped, exposed;
static unsigned long trials;

static void on_fault(int sig) { siglongjmp(fault_env, sig); }

static void pin(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    sched_setaffinity(0, sizeof(set), &set);
}

static long enter(unsigned long request) {
    struct keystone_ioctl_run_enclave arg = {eid, 0, 0};
    if (ioctl(dev, request, &arg))
        return -1;
    return (long)arg.error;
}

static long resume() { return enter(KEYSTONE_IOC_RESUME_ENCLAVE); }

/* resume until the enclave stops with the call id */
static long resume_to(unsigned long id) {
    for (int i = 0; i < 1000; i++) {
        long r = resume();
        if (r != KEYSTONE_ENCLAVE_EDGE_CALL_HOST && r != KEYSTONE_ENCLAVE_INTERRUPTED)
            return r;
        if (r == KEYSTONE_ENCLAVE_EDGE_CALL_HOST && call->call_id == id)
            return 0;
    }
    return -2;
}

/* bit c is set if CPU c can read the ADM */
static int peek_all(int home) {
    int mask = 0;
    for (int c = 0; c < NCPU; c++) {
        pin(c);
        if (sigsetjmp(fault_env, 1) == 0) {
            unsigned char v = *adm;
            (void)v;
            mask |= 1 << c;
        }
    }
    pin(home);
    return mask;
}

static void runner(int idx) {
    int cpu = idx + 1;
    pin(cpu);
    while (!done) {
        unsigned long round = round_no;
        long r              = resume();
        if (r == SBI_ERR_SM_ENCLAVE_NOT_RESUMABLE) {
            n_busy++;
            continue;
        }
        if (r != KEYSTONE_ENCLAVE_EDGE_CALL_HOST && r != KEYSTONE_ENCLAVE_INTERRUPTED) {
            printf("[host] runner %d: resume returned %ld\n", idx, r);
            failed = 1;
            done   = 1;
            break;
        }
        if (entered++ == 0) {
            /* first in this round: leave the enclave to the other thread */
            while (round_no == round && !done)
                sched_yield();
            continue;
        }
        /* second: the enclave is stopped and the other thread waits */
        if (call->call_id == OCALL_UNSHARED) {
            int mask = peek_all(cpu);
            if (mask && exposed++ < 10)
                printf(
                    "[host] check %lu: ADM readable without a share, cpus 0x%x\n",
                    checked.load() + 1, mask);
            if (++checked % 1000 == 0)
                printf("[host] %lu checks, %lu exposed\n", checked.load(), exposed.load());
            if (checked >= trials)
                done = 1;
        } else {
            skipped++;
            if (resume_to(OCALL_UNSHARED) != 0) {
                failed = 1;
                done   = 1;
            }
        }
        entered = 0;
        round_no++;
    }
}

int main(int argc, char** argv) {
    const char* env      = getenv("ADMRACE_TRIALS");
    trials               = env ? strtoul(env, nullptr, 0) : 2000;
    env                  = getenv("ADMRACE_SPIN");
    unsigned long spin   = env ? strtoul(env, nullptr, 0) : 1000;

    struct sigaction sa = {};
    sa.sa_handler       = on_fault;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    setvbuf(stdout, nullptr, _IOLBF, 0);

    Keystone::AdditionalData data;
    data.storeBytes((uint8_t*)&spin, sizeof(spin), 1);

    Keystone::Enclave enclave = Keystone::EnclaveBuilder()
                                    .workingMemory(256 * 1024)
                                    .sharedMemory(256 * 1024)
                                    .dataMemory(16 * 1024, ProtectionTypes::STRICT)
                                    .build();
    if (enclave.init(argv[1], argv[2], argv[3], data) != Keystone::Error::Success) {
        printf("[host] init failed\n");
        return 1;
    }
    adm  = (volatile unsigned char*)enclave.getAdditionalMemory();
    call = (volatile struct edge_call*)enclave.getSharedBuffer();
    eid  = enclave.getEid();
    dev  = open(KEYSTONE_DEV_PATH, O_RDWR);
    if (dev < 0) {
        printf("[host] cannot open the device\n");
        return 1;
    }

    /* without a race: readable while shared, not readable otherwise */
    pin(0);
    long r = enter(KEYSTONE_IOC_RUN_ENCLAVE);
    if (r != KEYSTONE_ENCLAVE_EDGE_CALL_HOST || call->call_id != OCALL_SHARED)
        r = resume_to(OCALL_SHARED);
    else
        r = 0;
    int shared = peek_all(0);
    if (r == 0)
        r = resume_to(OCALL_UNSHARED);
    if (r != 0) {
        printf("[host] the enclave did not make the calls: %ld\n", r);
        return 1;
    }
    int unshared = peek_all(0);
    printf("CONTROL shared=0x%x unshared=0x%x\n", shared, unshared);

    std::thread t0(runner, 0), t1(runner, 1);
    t0.join();
    t1.join();

    printf(
        "RESULT checked=%lu exposed=%lu skipped=%lu refused=%lu%s\n", checked.load(),
        exposed.load(), skipped.load(), n_busy.load(), failed ? " FAILED" : "");
    return exposed || failed ? 2 : 0;
}
