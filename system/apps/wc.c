#include "wc.h"
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

void wc_run(char *args) {
    vfs_split_word(args);

    if (args[0] == '\0') {
        console_print("usage: wc <file>\n");
        return;
    }

    const char *content = vfs_get_content(args);
    if (content == 0) {
        console_print("wc: no such file: ");
        console_print(args);
        console_print("\n");
        return;
    }

    unsigned int chars = 0;
    unsigned int words = 0;
    int in_word = 0;

    for (unsigned int i = 0; content[i] != '\0'; i++) {
        chars++;
        if (content[i] == ' ') {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }

    print_uint(chars);
    console_print(" chars, ");
    print_uint(words);
    console_print(" words\n");
}
