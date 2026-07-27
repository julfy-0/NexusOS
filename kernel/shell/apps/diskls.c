#include "diskls.h"
#include "console.h"
#include "fat32.h"

void diskls_run(const char *args) {
    const char *path = (args[0] == '\0') ? "/" : args;

    if (!fat32_is_mounted()) {
        console_print("diskls: no FAT32 disk mounted (see 'help')\n");
        return;
    }

    fat32_list(path);
}
