#include "dec.h"
#include "console.h"

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void print_uint(unsigned long n) {
    char digits[20];
    int i = 0;
    if (n == 0) {
        console_putchar('0');
        return;
    }
    while (n > 0 && i < 20) {
        digits[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    while (i > 0) {
        i--;
        console_putchar(digits[i]);
    }
}

void dec_run(char *args) {
    const char *s = args;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    if (s[0] == '\0') {
        console_print("usage: dec <hex number>\n");
        return;
    }

    unsigned long v = 0;
    for (int i = 0; s[i] != '\0'; i++) {
        int d = hex_digit(s[i]);
        if (d < 0) {
            console_print("dec: invalid hex digit\n");
            return;
        }
        v = v * 16 + (unsigned long)d;
    }

    print_uint(v);
    console_print("\n");
}
