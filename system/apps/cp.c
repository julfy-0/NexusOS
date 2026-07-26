#include "cp.h"
#include "vfs.h"
#include "console.h"

void cp_run(char *args) {
    char *dst = vfs_split_word(args);

    if (args[0] == '\0' || dst[0] == '\0') {
        console_print("usage: cp <src> <dst>\n");
        return;
    }

    if (vfs_cp(args, dst) != 0) {
        console_print("cp: cannot copy '");
        console_print(args);
        console_print("' to '");
        console_print(dst);
        console_print("' (src is not a file, dst exists, or out of space)\n");
    }
}
