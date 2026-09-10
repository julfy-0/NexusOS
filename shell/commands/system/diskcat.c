#include <stdint.h>
#include "diskcat.h"
#include "console.h"
#include "fat32.h"

#define DISKCAT_BUF_SIZE (64 * 1024)
static uint8_t g_buf[DISKCAT_BUF_SIZE];

void diskcat_run(const char *args) {
    if (!fat32_is_mounted()) {
        console_print("diskcat: no FAT32 disk mounted (see 'help')\n");
        return;
    }
    if (args[0] == '\0') {
        console_print("usage: diskcat <path>\n");
        return;
    }

    uint32_t size = 0;
    if (!fat32_read_file(args, g_buf, DISKCAT_BUF_SIZE - 1, &size)) {
        console_print("diskcat: cannot read file: ");
        console_print(args);
        console_print("\n");
        return;
    }

    g_buf[size] = '\0';
    console_print((const char *)g_buf);
    console_print("\n");

    if (size == DISKCAT_BUF_SIZE - 1) {
        console_set_color(COLOR_YELLOW, COLOR_BLACK);
        console_print("(вывод обрезан — файл больше 64 КБ)\n");
        console_set_color(COLOR_WHITE, COLOR_BLACK);
    }
}
