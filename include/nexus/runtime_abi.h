#ifndef NEXUSOS_RUNTIME_ABI_H
#define NEXUSOS_RUNTIME_ABI_H

#include <stdint.h>
#include "syscall_abi.h"

#define NEXUS_RUNTIME_ABI_VERSION 1u
#define NEXUS_RUNTIME_NAME "Nexus Runtime"

#define NEXUS_RUNTIME_FEATURE_IPC        (1ULL << 0)
#define NEXUS_RUNTIME_FEATURE_CHANNELS   (1ULL << 1)
#define NEXUS_RUNTIME_FEATURE_USER_ABI   (1ULL << 2)
#define NEXUS_RUNTIME_FEATURE_PRIVATE_CR3 (1ULL << 3)
#define NEXUS_RUNTIME_FEATURE_VFS        (1ULL << 4)

#define NEXUS_CHANNEL_INVALID 0u

typedef struct {
    uint32_t abi_version;
    uint32_t syscall_abi_version;
    uint32_t channel_limit;
    uint32_t message_size;
    uint64_t feature_bits;
    uint64_t process_pid;
    uint64_t process_parent_pid;
    uint64_t process_address_space;
    char name[32];
} nexus_runtime_info_t;

#endif
