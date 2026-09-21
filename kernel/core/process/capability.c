#include "capability.h"

const char *nexus_capability_name(uint64_t capability) {
    switch (capability) {
        case NEXUS_CAP_RUNTIME: return "runtime";
        case NEXUS_CAP_IPC: return "ipc";
        case NEXUS_CAP_FILESYSTEM_READ: return "filesystem.read";
        case NEXUS_CAP_FILESYSTEM_WRITE: return "filesystem.write";
        case NEXUS_CAP_PROCESS_SPAWN: return "process.spawn";
        case NEXUS_CAP_PROCESS_CONTROL: return "process.control";
        case NEXUS_CAP_MEMORY: return "memory";
        case NEXUS_CAP_SYSTEM_INFO: return "system.info";
        case NEXUS_CAP_DEVICE: return "device";
        case NEXUS_CAP_NETWORK: return "network";
        case NEXUS_CAP_ADMIN: return "admin";
        default: return "unknown";
    }
}

int nexus_capability_valid(uint64_t capability) {
    if (capability == 0 || (capability & (capability - 1)) != 0) return 0;
    return (capability & NEXUS_CAP_ALL) != 0;
}
