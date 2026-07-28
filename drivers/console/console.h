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

/* Строка состояния в духе классического Linux-boot: печатает текущей
 * позицией курсора выровненный к правому краю экрана статус вида
 * "[ OK ]" / "[FAIL]" / "[WARN]" и переводит строку. Использование:
 *   console_print("Кусок текста без \\n");
 *   <сделать шаг>
 *   console_status_ok();   // или _fail() / _warn()
 * Если текст слева уже длиннее доступного места — просто печатает
 * статус сразу после текста (без попытки обрезать или сломать строку). */
void console_status_ok(void);
void console_status_fail(void);
void console_status_warn(void);

/* Scrollback (PgUp/PgDn). delta в строках: положительный — листать назад
 * (к старым строкам), отрицательный — вперёд (к живому выводу). Любая
 * новая печать через console_putchar() сама возвращает к живому виду —
 * вызывающему (клавиатуре) не нужно об этом заботиться. */
void console_scroll(int32_t delta);
uint32_t console_get_rows(void);

/* Готовые цвета в формате 0x00RRGGBB (конвертация под пиксельный формат
 * экрана происходит внутри console.c) */
#define COLOR_BLACK   0x000000
#define COLOR_WHITE   0xFFFFFF
#define COLOR_RED     0xFF3B30
#define COLOR_GREEN   0x30D158
#define COLOR_CYAN    0x32ADE6
#define COLOR_YELLOW  0xFFD60A

#endif
