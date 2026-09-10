#include "init.h"
#include "vfs.h"
#include "mount.h"
#include "gpt.h"
#include "ahci.h"
#include "fat32.h"
#include "console.h"

#define BOOT_GUID    NEXUS_GPT_BOOT_TYPE
#define SYSTEM_GUID  NEXUS_GPT_SYSTEM_TYPE
#define USERDATA_GUID NEXUS_GPT_USERDATA_TYPE

static void mount_partition(const nexus_gpt_partition_t *p, int backend, const char *path) {
    if (!p) return;
    if (fat32_mount_partition(p->first_lba, backend)) {
        char source[32];
        source[0]='a'; source[1]='h'; source[2]='c'; source[3]='i'; source[4]='0'; source[5]='p';
        source[6]=(char)('0'+p->index); source[7]='\0';
        vfs_mount_backend(source, path, "fat32", VFS_MOUNT_RDONLY, backend);
        console_print("  -> "); console_print(p->name); console_print(" mounted at "); console_print(path); console_print("\n");
    } else {
        console_print("  -> "); console_print(p->name); console_print(" is not a mountable FAT32 volume\n");
    }
}

int nexus_system_bootstrap(void) {
    vfs_init();
    vfs_mount_init();

    if (!ahci_is_ready()) return 1;
    int count = gpt_scan();
    if (count <= 0) return 1;

    console_print("Nexus System storage discovery\n");
    const nexus_gpt_partition_t *boot = gpt_find_type(BOOT_GUID);
    const nexus_gpt_partition_t *system = gpt_find_type(SYSTEM_GUID);
    const nexus_gpt_partition_t *userdata = gpt_find_type(USERDATA_GUID);

    if (boot) mount_partition(boot, 0, "/boot");
    if (system) mount_partition(system, 1, "/system");
    if (userdata) mount_partition(userdata, 2, "/userdata");

    /* Preserve the historical disk path as an alias to the boot volume. */
    if (boot && fat32_select_mount(0)) {
        vfs_mount_backend("ahci0p1", "/mnt/disk0", "fat32", VFS_MOUNT_RDONLY, 0);
    }
    return 1;
}
