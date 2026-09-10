#ifndef NEXUSOS_MOUNT_H
#define NEXUSOS_MOUNT_H

#include <stdint.h>

#define VFS_PATH_MAX 256
#define VFS_FSTYPE_MAX 16
#define VFS_MOUNT_MAX 32

typedef enum {
    VFS_MOUNT_RDONLY = 1u << 0,
    VFS_MOUNT_NODEV  = 1u << 1,
    VFS_MOUNT_NOEXEC = 1u << 2,
    VFS_MOUNT_NOSUID = 1u << 3
} vfs_mount_flags_t;

typedef struct {
    int used;
    char source[VFS_PATH_MAX];
    char target[VFS_PATH_MAX];
    char fstype[VFS_FSTYPE_MAX];
    uint32_t flags;
    int backend; /* filesystem backend slot; -1 for namespace-only mounts */
} vfs_mount_t;

void vfs_mount_init(void);
int vfs_mount(const char *source, const char *target,
              const char *fstype, uint32_t flags);
int vfs_mount_backend(const char *source, const char *target,
                      const char *fstype, uint32_t flags, int backend);
int vfs_umount(const char *target);
const vfs_mount_t *vfs_mount_get(int index);
int vfs_mount_count(void);
int vfs_mount_resolve(const char *path, char *mountpoint_out, int out_size);
void vfs_mount_list(void);

#endif
