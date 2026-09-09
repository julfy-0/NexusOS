#include "mount.h"
#include "console.h"

static vfs_mount_t mounts[VFS_MOUNT_MAX];

static int str_eq(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == 0 && b[i] == 0;
}

static void str_copy(char *dst, const char *src, int max) {
    int i = 0;
    if (max <= 0) return;
    while (src && src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

void vfs_mount_init(void) {
    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        mounts[i].used = 0;
        mounts[i].source[0] = 0;
        mounts[i].target[0] = 0;
        mounts[i].fstype[0] = 0;
        mounts[i].flags = 0;
    }

    /* Linux-like NexusOS virtual filesystem layout. */
    vfs_mount("rootfs", "/", "nexusfs", 0);
    vfs_mount("devfs", "/dev", "devfs", 0);
    vfs_mount("procfs", "/proc", "procfs", VFS_MOUNT_NODEV);
    vfs_mount("sysfs", "/sys", "sysfs", VFS_MOUNT_NODEV);
    vfs_mount("tmpfs", "/tmp", "tmpfs", 0);
}

int vfs_mount(const char *source, const char *target,
              const char *fstype, uint32_t flags) {
    if (!source || !target || !fstype || !target[0]) return -1;

    /* Replace an existing mount at the same target. */
    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        if (mounts[i].used && str_eq(mounts[i].target, target)) {
            str_copy(mounts[i].source, source, VFS_PATH_MAX);
            str_copy(mounts[i].fstype, fstype, VFS_FSTYPE_MAX);
            mounts[i].flags = flags;
            return i;
        }
    }

    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        if (!mounts[i].used) {
            mounts[i].used = 1;
            str_copy(mounts[i].source, source, VFS_PATH_MAX);
            str_copy(mounts[i].target, target, VFS_PATH_MAX);
            str_copy(mounts[i].fstype, fstype, VFS_FSTYPE_MAX);
            mounts[i].flags = flags;
            return i;
        }
    }
    return -2;
}

int vfs_umount(const char *target) {
    if (!target) return -1;

    /* Keep the root mount present. */
    if (str_eq(target, "/")) return -2;

    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        if (mounts[i].used && str_eq(mounts[i].target, target)) {
            mounts[i].used = 0;
            return 0;
        }
    }
    return -1;
}

const vfs_mount_t *vfs_mount_get(int index) {
    if (index < 0 || index >= VFS_MOUNT_MAX || !mounts[index].used)
        return 0;
    return &mounts[index];
}

int vfs_mount_count(void) {
    int n = 0;
    for (int i = 0; i < VFS_MOUNT_MAX; i++)
        if (mounts[i].used) n++;
    return n;
}

int vfs_mount_resolve(const char *path, char *mountpoint_out, int out_size) {
    if (!path || !mountpoint_out || out_size <= 0) return -1;

    int best = -1;
    int best_len = -1;

    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        if (!mounts[i].used) continue;

        int len = 0;
        while (mounts[i].target[len]) len++;

        int match = 1;
        for (int j = 0; j < len; j++) {
            if (path[j] != mounts[i].target[j]) {
                match = 0;
                break;
            }
        }

        if (match && len > best_len &&
            (path[len] == 0 || path[len] == '/')) {
            best = i;
            best_len = len;
        }
    }

    if (best < 0) return -1;
    str_copy(mountpoint_out, mounts[best].target, out_size);
    return best;
}

void vfs_mount_list(void) {
    console_print("\nMount points:\n");
    for (int i = 0; i < VFS_MOUNT_MAX; i++) {
        if (!mounts[i].used) continue;
        console_print("  ");
        console_print(mounts[i].source);
        console_print(" -> ");
        console_print(mounts[i].target);
        console_print(" [");
        console_print(mounts[i].fstype);
        console_print("]\n");
    }
}
