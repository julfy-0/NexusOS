#include "runtime.h"
#include "process.h"

static int g_ready;

int nexus_runtime_init(void) {
    g_ready = 1;
    return 1;
}

int nexus_runtime_ready(void) { return g_ready; }
const char *nexus_runtime_name(void) { return NEXUS_RUNTIME_NAME; }
uint32_t nexus_runtime_abi_version(void) { return NEXUS_RUNTIME_ABI_VERSION; }

int nexus_runtime_fill_info(uint64_t pid, nexus_runtime_info_t *out) {
    nexus_process_t *p;
    if (!g_ready || !out) return 0;
    p = process_get(pid);
    if (!p || p->state == PROCESS_ZOMBIE) return 0;

    out->abi_version = NEXUS_RUNTIME_ABI_VERSION;
    out->syscall_abi_version = NEXUS_SYSCALL_ABI_VERSION;
    out->channel_limit = NEXUS_PROCESS_CHANNELS;
    out->message_size = NEXUS_CHANNEL_MESSAGE_MAX;
    out->feature_bits = NEXUS_RUNTIME_FEATURE_IPC |
                        NEXUS_RUNTIME_FEATURE_CHANNELS |
                        NEXUS_RUNTIME_FEATURE_USER_ABI |
                        NEXUS_RUNTIME_FEATURE_PRIVATE_CR3 |
                        NEXUS_RUNTIME_FEATURE_VFS;
    out->process_pid = p->pid;
    out->process_parent_pid = p->parent_pid;
    out->process_address_space = p->address_space_cr3;

    for (uint32_t i = 0; i < sizeof(out->name) - 1; ++i) {
        char c = NEXUS_RUNTIME_NAME[i];
        out->name[i] = c;
        if (c == 0) return 1;
    }
    out->name[sizeof(out->name) - 1] = 0;
    return 1;
}
