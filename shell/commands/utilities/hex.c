#include "hex.h"
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

void hex_run(char *args) {
    long v;
    if (!parse_int(args, &v)) {
        console_print("usage: hex <decimal number>\n");
        return;
    }

    unsigned long uv = (unsigned long)v;
    if (v < 0) {
        console_print("-");
        uv = (unsigned long)(-v);
    }

    console_print("0x");
    char buf[17];
    const char *digits = "0123456789ABCDEF";
    int i = 16;
    buf[i] = '\0';
    if (uv == 0) {
        console_print("0\n");
        return;
    }
    while (uv > 0 && i > 0) {
        i--;
        buf[i] = digits[uv & 0xF];
        uv >>= 4;
    }
    console_print(&buf[i]);
    console_print("\n");
}
