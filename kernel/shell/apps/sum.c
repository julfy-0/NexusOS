#include "sum.h"
#include "vfs.h"
#include "console.h"

static int parse_int(const char *s, long *out) {
    int neg = 0;
    long v = 0;
    int got = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
        got = 1;
    }
    *out = neg ? -v : v;
    return got;
}

void sum_run(char *args) {
    if (args[0] == '\0') {
        console_print("usage: sum <n1> <n2> ...\n");
        return;
    }

    long total = 0;
    char *rest = args;

    while (rest[0] != '\0') {
        char *next = vfs_split_word(rest);
        long v;
        if (parse_int(rest, &v)) {
            total += v;
        }
        rest = next;
    }

    if (total < 0) {
        console_print("-");
        total = -total;
    }

    char buf[24];
    int i = 0;
    unsigned long uv = (unsigned long)total;
    if (uv == 0) {
        console_print("0\n");
        return;
    }
    while (uv > 0 && i < 24) {
        buf[i++] = (char)('0' + (uv % 10));
        uv /= 10;
    }
    while (i > 0) {
        i--;
        console_putchar(buf[i]);
    }
    console_print("\n");
}
