#include "fib.h"
#include "console.h"

static int parse_uint(const char *s, unsigned long *out) {
    unsigned long v = 0;
    int got = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (unsigned long)(*s - '0');
        s++;
        got = 1;
    }
    *out = v;
    return got;
}

static void print_uint(unsigned long n) {
    char digits[24];
    int i = 0;
    if (n == 0) {
        console_putchar('0');
        return;
    }
    while (n > 0 && i < 24) {
        digits[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    while (i > 0) {
        i--;
        console_putchar(digits[i]);
    }
}

void fib_run(char *args) {
    unsigned long n;
    if (!parse_uint(args, &n)) {
        console_print("usage: fib <n>\n");
        return;
    }
    if (n > 90) {
        console_print("fib: n too large (max 90, would overflow 64 bit)\n");
        return;
    }

    unsigned long a = 0, b = 1;
    for (unsigned long i = 0; i < n; i++) {
        unsigned long next = a + b;
        a = b;
        b = next;
    }

    print_uint(a);
    console_print("\n");
}
