#include "mv.h"
#include "vfs.h"
#include "console.h"

void mv_run(char *args) {
    char *dst = vfs_split_word(args);

    if (args[0] == '\0' || dst[0] == '\0') {
        console_print("usage: mv <src> <dst>\n");
        return;
    }

    if (vfs_mv(args, dst) != 0) {
        console_print("mv: cannot rename '");
        console_print(args);
        console_print("' to '");
        console_print(dst);
        console_print("' (not found or dst already exists)\n");
    }
}
