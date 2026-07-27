#include "isprime.h"
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

void isprime_run(char *args) {
    unsigned long n;
    if (!parse_uint(args, &n)) {
        console_print("usage: isprime <n>\n");
        return;
    }

    if (n < 2) {
        console_print("not prime\n");
        return;
    }

    int prime = 1;
    for (unsigned long i = 2; i * i <= n; i++) {
        if (n % i == 0) {
            prime = 0;
            break;
        }
    }

    console_print(prime ? "prime\n" : "not prime\n");
}
