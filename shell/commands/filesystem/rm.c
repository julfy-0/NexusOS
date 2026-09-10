#include "rm.h"
#include "vfs.h"
#include "console.h"

void rm_run(char *args) {
    vfs_split_word(args);

    if (args[0] == '\0') {
        console_print("usage: rm <name>\n");
        return;
    }

    if (vfs_rm(args) != 0) {
        console_print("rm: cannot remove '");
        console_print(args);
        console_print("' (not found or directory not empty)\n");
    }
}
