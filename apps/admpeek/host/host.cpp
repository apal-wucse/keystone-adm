// Host access to the ADM from a restarted hart (MODEL.typ Section 8 #7).
// Creates an enclave, whose ADM the host must not access, restarts CPU
// $ADMPEEK_CPU (default 1) with CPU hotplug, and reads the ADM from it.
// Run as root.
#include <csetjmp>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <sched.h>

#include "host/keystone.h"

static sigjmp_buf fault_env;

static void on_fault(int sig) { siglongjmp(fault_env, sig); }

static int set_online(int cpu, int on) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online", cpu);
    FILE* f = fopen(path, "w");
    if (!f)
        return -1;
    fprintf(f, "%d", on);
    return fclose(f);
}

/* 1: readable, 0: access fault, -1: error */
static int peek(int cpu, volatile unsigned char* p) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set))
        return -1;

    int sig = sigsetjmp(fault_env, 1);
    if (sig == 0) {
        unsigned char v = *p;
        printf("[host] cpu %d: ADM readable (first byte 0x%02x)\n", cpu, v);
        return 1;
    }
    printf("[host] cpu %d: access to the ADM faulted (signal %d)\n", cpu, sig);
    return 0;
}

int main(int argc, char** argv) {
    const char* env = getenv("ADMPEEK_CPU");
    int cpu         = env ? atoi(env) : 1;

    struct sigaction sa = {};
    sa.sa_handler       = on_fault;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);

    Keystone::AdditionalData data;
    unsigned char bytes[64];
    for (size_t i = 0; i < sizeof(bytes); i++)
        bytes[i] = 0x5a;
    data.storeBytes(bytes, sizeof(bytes), 1);

    Keystone::Enclave enclave = Keystone::EnclaveBuilder()
                                    .workingMemory(256 * 1024)
                                    .sharedMemory(256 * 1024)
                                    .dataMemory(16 * 1024, ProtectionTypes::STRICT)
                                    .build();
    if (enclave.init(argv[1], argv[2], argv[3], data) != Keystone::Error::Success) {
        printf("[host] init failed\n");
        return 1;
    }
    volatile unsigned char* adm = (volatile unsigned char*)enclave.getAdditionalMemory();

    int before = peek(cpu, adm);
    cpu_set_t all;
    CPU_ZERO(&all);
    CPU_SET(0, &all);
    sched_setaffinity(0, sizeof(all), &all);
    if (set_online(cpu, 0) || set_online(cpu, 1)) {
        printf("[host] CPU hotplug of cpu %d failed\n", cpu);
        return 1;
    }
    printf("[host] cpu %d restarted\n", cpu);
    int after = peek(cpu, adm);

    printf(
        "RESULT before=%s after=%s\n", before == 0 ? "protected" : "EXPOSED",
        after == 0 ? "protected" : "EXPOSED");
    return before == 0 && after == 0 ? 0 : 2;
}
