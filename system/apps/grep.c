#include "grep.h"
#include "vfs.h"
#include "console.h"

static int contains(const char *haystack, const char *needle) {
    if (needle[0] == '\0') {
        return 1;
    }

    for (int i = 0; haystack[i] != '\0'; i++) {
        int j = 0;
        while (haystack[i + j] != '\0' && needle[j] != '\0' &&
               haystack[i + j] == needle[j]) {
            j++;
        }
        if (needle[j] == '\0') {
            return 1;
        }
    }
    return 0;
}

void grep_run(char *args) {
    char *file = vfs_split_word(args);

    if (args[0] == '\0' || file[0] == '\0') {
        console_print("usage: grep <word> <file>\n");
        return;
    }

    const char *content = vfs_get_content(file);
    if (content == 0) {
        console_print("grep: no such file: ");
        console_print(file);
        console_print("\n");
        return;
    }

    if (contains(content, args)) {
        console_print(content);
        console_print("\n");
    } else {
        console_print("grep: no match\n");
    }
}
