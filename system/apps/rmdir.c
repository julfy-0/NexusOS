#include "rmdir.h"
#include "vfs.h"
#include "console.h"

void rmdir_run(char *args) {
    vfs_split_word(args);

    if (args[0] == '\0') {
        console_print("usage: rmdir <dir>\n");
        return;
    }

    if (vfs_rmdir(args) != 0) {
        console_print("rmdir: cannot remove '");
        console_print(args);
        console_print("' (not a directory, not found, or not empty)\n");
    }
}
