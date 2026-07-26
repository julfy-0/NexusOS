#include "len.h"
#include "console.h"

#define LEN_MAX 255

/* Печатает беззнаковое число в десятичном виде без printf/sprintf. */
static void print_uint(unsigned int n) {
    char digits[10];
    int i = 0;

    if (n == 0) {
        console_putchar('0');
        return;
    }

    while (n > 0 && i < 10) {
        digits[i++] = (char)('0' + (n % 10));
        n /= 10;
    }

    while (i > 0) {
        i--;
        console_putchar(digits[i]);
    }
}

void len_run(const char *args) {
    unsigned int len = 0;
    while (args[len] != '\0' && len < LEN_MAX) {
        len++;
    }

    print_uint(len);
    console_putchar('\n');
}
