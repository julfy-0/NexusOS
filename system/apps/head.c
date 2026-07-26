#include "head.h"
#include "vfs.h"
#include "console.h"

static int parse_uint(const char *s, unsigned int *out) {
    unsigned int v = 0;
    int got = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (unsigned int)(*s - '0');
        s++;
        got = 1;
    }
    *out = v;
    return got;
}

void head_run(char *args) {
    char *n_str = vfs_split_word(args);

    if (args[0] == '\0') {
        console_print("usage: head <file> [n]\n");
        return;
    }

    unsigned int n = 20;
    if (n_str[0] != '\0') {
        parse_uint(n_str, &n);
    }

    const char *content = vfs_get_content(args);
    if (content == 0) {
        console_print("head: no such file: ");
        console_print(args);
        console_print("\n");
        return;
    }

    for (unsigned int i = 0; content[i] != '\0' && i < n; i++) {
        console_putchar(content[i]);
    }
    console_putchar('\n');
}
