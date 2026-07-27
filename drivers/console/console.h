#ifndef NEXUSOS_CONSOLE_H
#define NEXUSOS_CONSOLE_H

#include <stdint.h>
#include "boot_info.h"

void console_init(nexus_framebuffer_t *fb);
void console_clear(void);
void console_set_color(uint32_t fg, uint32_t bg);
void console_putchar(char c);
void console_print(const char *s);
void console_print_hex(uint64_t value);
void console_print_dec(uint64_t value);

/* Готовые цвета в формате 0x00RRGGBB (конвертация под пиксельный формат
 * экрана происходит внутри console.c) */
#define COLOR_BLACK   0x000000
#define COLOR_WHITE   0xFFFFFF
#define COLOR_RED     0xFF3B30
#define COLOR_GREEN   0x30D158
#define COLOR_CYAN    0x32ADE6
#define COLOR_YELLOW  0xFFD60A

#endif
