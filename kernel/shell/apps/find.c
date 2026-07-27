#include "find.h"
#include "vfs.h"
#include "console.h"

void find_run(char *args) {
    vfs_split_word(args);

    if (args[0] == '\0') {
        console_print("usage: find <name>\n");
        return;
    }

    vfs_find(args);
}
