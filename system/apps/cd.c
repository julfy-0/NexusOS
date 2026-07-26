#include "cd.h"
#include "vfs.h"
#include "console.h"

void cd_run(char *args) {
    vfs_split_word(args);

    if (args[0] == '\0') {
        console_print("usage: cd <dir>\n");
        return;
    }

    if (vfs_cd(args) != 0) {
        console_print("cd: no such directory: ");
        console_print(args);
        console_print("\n");
    }
}
