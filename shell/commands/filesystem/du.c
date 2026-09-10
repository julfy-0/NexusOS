#include "du.h"
#include "vfs.h"
#include "console.h"

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

void du_run(void) {
    unsigned int bytes, files;
    vfs_du(&bytes, &files);

    print_uint(bytes);
    console_print(" bytes in ");
    print_uint(files);
    console_print(" file(s)\n");
}
