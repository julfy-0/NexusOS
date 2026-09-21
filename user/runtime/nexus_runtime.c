#include "nexus_runtime.h"

long nexus_syscall6(long number, long a0, long a1, long a2, long a3, long a4, long a5) {
    register long r10 __asm__("r10") = a3;
    register long r8  __asm__("r8") = a4;
    register long r9  __asm__("r9") = a5;
    long result;
    __asm__ volatile (
        "int $0x80"
        : "=a"(result)
        : "0"(number), "D"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory");
    return result;
}

long nexus_runtime_info(nexus_runtime_info_t *out) {
    return nexus_syscall6(NEXUS_SYS_RUNTIME_INFO, (long)out, (long)sizeof(*out), 0, 0, 0, 0);
}

long nexus_channel_create(uint64_t peer_pid) {
    return nexus_syscall6(NEXUS_SYS_CHANNEL_CREATE, (long)peer_pid, 0, 0, 0, 0, 0);
}

long nexus_channel_close(uint32_t handle) {
    return nexus_syscall6(NEXUS_SYS_CHANNEL_CLOSE, (long)handle, 0, 0, 0, 0, 0);
}

long nexus_channel_send(uint32_t handle, const void *data, uint32_t size) {
    return nexus_syscall6(NEXUS_SYS_CHANNEL_SEND, (long)handle, (long)data, (long)size, 0, 0, 0);
}

long nexus_channel_recv(uint32_t handle, void *data, uint32_t capacity, uint32_t *out_size) {
    return nexus_syscall6(NEXUS_SYS_CHANNEL_RECV, (long)handle, (long)data, (long)capacity, (long)out_size, 0, 0);
}

long nexus_channel_poll(uint32_t handle) {
    return nexus_syscall6(NEXUS_SYS_CHANNEL_POLL, (long)handle, 0, 0, 0, 0, 0);
}

long nexus_capabilities(uint64_t *out_mask) {
    return nexus_syscall6(NEXUS_SYS_GETCAPS, (long)out_mask, (long)sizeof(*out_mask), 0, 0, 0, 0);
}

long nexus_has_capability(uint64_t capability) {
    return nexus_syscall6(NEXUS_SYS_HAS_CAP, (long)capability, 0, 0, 0, 0, 0);
}

long nexus_drop_capability(uint64_t capability) {
    return nexus_syscall6(NEXUS_SYS_DROP_CAP, (long)capability, 0, 0, 0, 0, 0);
}
