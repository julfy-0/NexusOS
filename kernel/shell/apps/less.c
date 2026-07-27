#include "less.h"
#include "vfs.h"
#include "console.h"

void less_run(char *args) {
    vfs_split_word(args);

    if (args[0] == '\0') {
        console_print("usage: less <file>\n");
        return;
    }

    vfs_cat(args);
}
