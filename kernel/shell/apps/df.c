#include "df.h"
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

void df_run(void) {
    unsigned int used, total;
    vfs_df(&used, &total);

    console_print("ramfs: ");
    print_uint(used);
    console_print(" / ");
    print_uint(total);
    console_print(" nodes used\n");
}
