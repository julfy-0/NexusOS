#include "mkdir.h"
#include "vfs.h"
#include "console.h"

void mkdir_run(char *args) {
    vfs_split_word(args); /* отбрасываем всё после первого слова */

    if (args[0] == '\0') {
        console_print("usage: mkdir <name>\n");
        return;
    }

    if (vfs_mkdir(args) != 0) {
        console_print("mkdir: cannot create '");
        console_print(args);
        console_print("' (already exists or out of space)\n");
    }
}
