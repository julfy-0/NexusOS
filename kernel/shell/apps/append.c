#include "append.h"
#include "vfs.h"
#include "console.h"

void append_run(char *args) {
    char *content = vfs_split_word(args);

    if (args[0] == '\0') {
        console_print("usage: append <file> <text>\n");
        return;
    }

    if (vfs_append(args, content) != 0) {
        console_print("append: cannot append to '");
        console_print(args);
        console_print("' (is a directory or out of space)\n");
    }
}
