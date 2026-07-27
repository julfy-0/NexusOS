#include "cat.h"
#include "vfs.h"
#include "console.h"

void cat_run(char *args) {
    vfs_split_word(args);

    if (args[0] == '\0') {
        console_print("usage: cat <file>\n");
        return;
    }

    vfs_cat(args);
}
