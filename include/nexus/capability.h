#ifndef NEXUSOS_CAPABILITY_H
#define NEXUSOS_CAPABILITY_H

#include <stdint.h>

/* Native NexusOS process capabilities. The bitmap is intentionally compact so
 * capability state is part of the process object, not a Unix-style FD table. */
typedef uint64_t nexus_capability_mask_t;

enum nexus_capability {
    NEXUS_CAP_RUNTIME       = 1ULL << 0,
    NEXUS_CAP_IPC           = 1ULL << 1,
    NEXUS_CAP_FILESYSTEM_READ  = 1ULL << 2,
    NEXUS_CAP_FILESYSTEM_WRITE = 1ULL << 3,
    NEXUS_CAP_PROCESS_SPAWN = 1ULL << 4,
    NEXUS_CAP_PROCESS_CONTROL = 1ULL << 5,
    NEXUS_CAP_MEMORY        = 1ULL << 6,
    NEXUS_CAP_SYSTEM_INFO   = 1ULL << 7,
    NEXUS_CAP_DEVICE        = 1ULL << 8,
    NEXUS_CAP_NETWORK       = 1ULL << 9,
    NEXUS_CAP_ADMIN         = 1ULL << 10
};

#define NEXUS_CAP_DEFAULT_USER \
    (NEXUS_CAP_RUNTIME | NEXUS_CAP_IPC | NEXUS_CAP_FILESYSTEM_READ | \
     NEXUS_CAP_FILESYSTEM_WRITE | NEXUS_CAP_PROCESS_SPAWN | \
     NEXUS_CAP_PROCESS_CONTROL | NEXUS_CAP_MEMORY | NEXUS_CAP_SYSTEM_INFO)

#define NEXUS_CAP_ALL \
    (NEXUS_CAP_DEFAULT_USER | NEXUS_CAP_DEVICE | NEXUS_CAP_NETWORK | NEXUS_CAP_ADMIN)

const char *nexus_capability_name(uint64_t capability);
int nexus_capability_valid(uint64_t capability);

#endif
