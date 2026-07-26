#include "diff.h"
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

void diff_run(char *args) {
    char *file2 = vfs_split_word(args);

    if (args[0] == '\0' || file2[0] == '\0') {
        console_print("usage: diff <file1> <file2>\n");
        return;
    }

    const char *c1 = vfs_get_content(args);
    const char *c2 = vfs_get_content(file2);

    if (c1 == 0 || c2 == 0) {
        console_print("diff: file not found\n");
        return;
    }

    unsigned int i = 0;
    while (c1[i] != '\0' && c2[i] != '\0' && c1[i] == c2[i]) {
        i++;
    }

    if (c1[i] == '\0' && c2[i] == '\0') {
        console_print("files are identical\n");
    } else {
        console_print("files differ at position ");
        print_uint(i);
        console_print("\n");
    }
}
