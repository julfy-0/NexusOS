#include "write.h"
#include "vfs.h"
#include "console.h"

void write_run(char *args) {
    char *content = vfs_split_word(args); /* args = имя файла, content = остальной текст */

    if (args[0] == '\0') {
        console_print("usage: write <file> <text>\n");
        return;
    }

    if (vfs_write(args, content) != 0) {
        console_print("write: cannot write '");
        console_print(args);
        console_print("' (out of space or is a directory)\n");
    }
}
