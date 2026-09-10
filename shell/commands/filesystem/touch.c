#include "touch.h"
#include "vfs.h"
#include "console.h"

void touch_run(char *args) {
    vfs_split_word(args);

    if (args[0] == '\0') {
        console_print("usage: touch <name>\n");
        return;
    }

    if (vfs_touch(args) != 0) {
        console_print("touch: cannot create '");
        console_print(args);
        console_print("' (already exists or out of space)\n");
    }
}
