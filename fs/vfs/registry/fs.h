#ifndef NEXUSOS_FS_H
#define NEXUSOS_FS_H

#include <stdint.h>

typedef struct nexus_fs {
    const char *name;
    int (*mount)(const char *source, const char *target, uint32_t flags);
    int (*unmount)(const char *target);
} nexus_fs_t;

void nexus_fs_init(void);
int nexus_fs_register(const nexus_fs_t *fs);
const nexus_fs_t *nexus_fs_find(const char *name);

#endif
