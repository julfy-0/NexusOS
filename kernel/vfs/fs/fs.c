#include "fs.h"

#define NEXUS_FS_MAX 16
static const nexus_fs_t *registry[NEXUS_FS_MAX];

static int eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == 0 && b[i] == 0;
}

void nexus_fs_init(void) {
    for (int i = 0; i < NEXUS_FS_MAX; i++) registry[i] = 0;
}

int nexus_fs_register(const nexus_fs_t *fs) {
    if (!fs || !fs->name) return -1;
    for (int i = 0; i < NEXUS_FS_MAX; i++) {
        if (!registry[i]) {
            registry[i] = fs;
            return i;
        }
    }
    return -2;
}

const nexus_fs_t *nexus_fs_find(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < NEXUS_FS_MAX; i++)
        if (registry[i] && eq(registry[i]->name, name))
            return registry[i];
    return 0;
}
