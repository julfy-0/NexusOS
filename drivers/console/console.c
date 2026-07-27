/* NexusOS: простейшая текстовая консоль поверх линейного framebuffer.
 * Никакого аппаратного ускорения — просто пишем пиксели напрямую. */
#include <stddef.h>
#include "console.h"
#include "font8x16.h"

extern void *memmove(void *dest, const void *src, unsigned long n);
extern void *memset(void *dest, int value, unsigned long n);

/* Реализация — ниже; нужны здесь, потому что console_init()/console_clear()
 * (сразу после pack_color/put_pixel) вызывают их раньше места определения. */
static void hist_clear_line(uint64_t logical_line);
static void hist_new_line(void);

static nexus_framebuffer_t *g_fb;
static uint32_t g_fg = 0xFFFFFF;
static uint32_t g_bg = 0x000000;
static uint32_t g_col = 0;
static uint32_t g_row = 0;
static uint32_t g_cols;
static uint32_t g_rows;

/* --- Scrollback (PgUp/PgDn) ---
 *
 * Раньше консоль просто двигала пиксели в framebuffer (см. scroll_if_needed)
 * и то, что уезжало за верхний край, терялось навсегда — окно в кадровый
 * буфер, а не в какой-то более долгоживущий текст.
 *
 * g_history[] — кольцевой буфер СИМВОЛОВ (а не пикселей): каждая когда-либо
 * начатая строка экрана — это одна запись, с цветом каждой ячейки (цвет мог
 * меняться посреди строки — приглашение шелла печатается другим цветом,
 * чем ввод). g_total_lines — монотонный счётчик "какая по счёту строка
 * сейчас пишется" — по нему же вычисляется индекс в кольце (% SCROLLBACK_LINES)
 * и определяется, не была ли нужная строка уже перезаписана новыми (кольцо
 * конечного размера).
 *
 * MAX_COLS — верхняя граница на ширину строки в ячейках; реальная g_cols
 * (зависит от разрешения GOP на конкретной машине) в это разумно
 * укладывается. Само разрешение неизвестно на этапе компиляции, поэтому
 * буфер фиксированного размера, а не по месту (тем более что kmalloc/heap
 * пока не существует — см. docs/ROADMAP.md). */
#define MAX_COLS 256
#define SCROLLBACK_LINES 500

typedef struct {
    char ch;
    uint32_t fg; /* уже "запакованный" под пиксельный формат цвет (см. pack_color) */
    uint32_t bg;
} console_cell_t;

static console_cell_t g_history[SCROLLBACK_LINES][MAX_COLS];
static uint64_t g_total_lines = 0;   /* номер строки, которая сейчас пишется (открыта) */
static uint32_t g_scroll_offset = 0; /* 0 = живой вид; N = N строк назад от живого края */

static uint32_t pack_color(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;
    if (g_fb->pixel_format == NEXUS_PIXFMT_BGR) {
        return (uint32_t)b << 16 | (uint32_t)g << 8 | (uint32_t)r;
    }
    /* RGB и "прочее" (bitmask-форматы почти всегда тоже RGB на практике) */
    return (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
}

static inline void put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= g_fb->width || y >= g_fb->height) return;
    uint32_t *row = (uint32_t *)(uintptr_t)(g_fb->base + (uint64_t)y * g_fb->pixels_per_scanline * 4);
    row[x] = color;
}

void console_init(nexus_framebuffer_t *fb) {
    g_fb = fb;
    g_cols = g_fb->width / FONT_WIDTH;
    g_rows = g_fb->height / FONT_HEIGHT;

    g_total_lines = 0;
    g_scroll_offset = 0;
    for (uint64_t i = 0; i < SCROLLBACK_LINES; i++) hist_clear_line(i);

    console_clear();
}

void console_clear(void) {
    /* Фиксируем в истории то, что было напечатано на текущей (ещё открытой)
     * строке до очистки экрана, и открываем чистую строку — иначе новый
     * текст после clear дописывался бы в ТУ ЖЕ ячейку истории поверх
     * старого содержимого. Сама история при этом не стирается: 'clear'
     * чистит видимый экран, а не scrollback — как в большинстве терминалов. */
    hist_new_line();
    g_scroll_offset = 0;

    g_col = 0;
    g_row = 0;
    uint32_t bg = pack_color(g_bg);
    for (uint32_t y = 0; y < g_fb->height; y++) {
        uint32_t *row = (uint32_t *)(uintptr_t)(g_fb->base + (uint64_t)y * g_fb->pixels_per_scanline * 4);
        for (uint32_t x = 0; x < g_fb->width; x++) row[x] = bg;
    }
}

void console_set_color(uint32_t fg, uint32_t bg) {
    g_fg = fg;
    g_bg = bg;
}

/* draw_glyph_raw принимает УЖЕ запакованные цвета — нужно для перерисовки
 * из истории (mm/scrollback), где у каждой ячейки свой сохранённый цвет,
 * а не текущий глобальный g_fg/g_bg. draw_glyph() — обычный путь печати
 * "прямо сейчас", просто пакует текущие глобальные цвета и зовёт raw. */
static void draw_glyph_raw(uint32_t col, uint32_t row, char c, uint32_t fg, uint32_t bg) {
    const uint8_t *glyph;
    if (c >= FONT_FIRST_CHAR && c <= FONT_LAST_CHAR) {
        glyph = font8x16[(uint8_t)c - FONT_FIRST_CHAR];
    } else {
        glyph = font8x16[0]; /* неизвестный символ (в т.ч. ch==0 пустой ячейки) -> пробел */
    }

    uint32_t px0 = col * FONT_WIDTH;
    uint32_t py0 = row * FONT_HEIGHT;

    for (uint32_t y = 0; y < FONT_HEIGHT; y++) {
        uint8_t bits = glyph[y];
        for (uint32_t x = 0; x < FONT_WIDTH; x++) {
            int on = (bits >> (FONT_WIDTH - 1 - x)) & 1;
            put_pixel(px0 + x, py0 + y, on ? fg : bg);
        }
    }
}

static void draw_glyph(uint32_t col, uint32_t row, char c) {
    draw_glyph_raw(col, row, c, pack_color(g_fg), pack_color(g_bg));
}

/* --- scrollback: запись в историю и перерисовка из неё --- */

static inline uint64_t hist_slot(uint64_t logical_line) {
    return logical_line % SCROLLBACK_LINES;
}

/* Затирает строку в истории пробелами текущим цветом фона — вызывается,
 * когда строка только начинается, чтобы после переиспользования кольца не
 * показывать мусор с многих оборотов назад. */
static void hist_clear_line(uint64_t logical_line) {
    console_cell_t *row = g_history[hist_slot(logical_line)];
    uint32_t bg = pack_color(g_bg);
    for (uint32_t x = 0; x < MAX_COLS; x++) {
        row[x].ch = ' ';
        row[x].fg = pack_color(g_fg);
        row[x].bg = bg;
    }
}

/* Записывает один символ в открытую (текущую) строку истории на позицию col. */
static void hist_put(uint32_t col, char c) {
    if (col >= MAX_COLS) return;
    console_cell_t *row = g_history[hist_slot(g_total_lines)];
    row[col].ch = c;
    row[col].fg = pack_color(g_fg);
    row[col].bg = pack_color(g_bg);
}

/* Закрывает текущую строку и открывает следующую (с чистого листа). */
static void hist_new_line(void) {
    g_total_lines++;
    hist_clear_line(g_total_lines);
}

/* Рисует одну физическую строку экрана (screen_row, 0..g_rows-1) по данным
 * логической строки истории. valid=0 — строки ещё не существовало (выше
 * самой первой когда-либо напечатанной) или она уже была затёрта кольцом —
 * рисуем просто фон. */
static void render_history_row(uint32_t screen_row, uint64_t logical_line, int valid) {
    uint32_t bg = pack_color(g_bg);
    for (uint32_t x = 0; x < g_cols && x < MAX_COLS; x++) {
        if (valid) {
            console_cell_t *cell = &g_history[hist_slot(logical_line)][x];
            draw_glyph_raw(x, screen_row, cell->ch, cell->fg, cell->bg);
        } else {
            draw_glyph_raw(x, screen_row, ' ', bg, bg);
        }
    }
}

/* Перерисовывает весь экран из истории с учётом g_scroll_offset. Вызывается
 * только по факту скролла (PgUp/PgDn) — не на каждый символ, так что цена
 * "перерисовать весь экран" тут не проблема (в отличие от scroll_if_needed,
 * который остаётся быстрым memmove-путём для обычной живой печати). */
static void console_redraw(void) {
    for (uint32_t r = 0; r < g_rows; r++) {
        int64_t logical = (int64_t)g_total_lines - (int64_t)g_scroll_offset
                           - (int64_t)(g_rows - 1 - r);
        int valid = logical >= 0 &&
                    (g_total_lines - (uint64_t)logical) < SCROLLBACK_LINES;
        render_history_row(r, valid ? (uint64_t)logical : 0, valid);
    }
}

void console_scroll(int32_t delta) {
    if (g_fb == NULL) return; /* до console_init() ещё нечего скроллить */

    int64_t new_offset = (int64_t)g_scroll_offset + delta;

    uint64_t max_offset = g_total_lines;
    if (max_offset > SCROLLBACK_LINES - 1) max_offset = SCROLLBACK_LINES - 1;

    if (new_offset < 0) new_offset = 0;
    if ((uint64_t)new_offset > max_offset) new_offset = (int64_t)max_offset;

    if ((uint32_t)new_offset == g_scroll_offset) return; /* уже на границе, перерисовывать нечего */

    g_scroll_offset = (uint32_t)new_offset;
    console_redraw();
}

uint32_t console_get_rows(void) {
    return g_rows;
}

static void scroll_if_needed(void) {
    if (g_row < g_rows) return;

    /* Сдвигаем всё содержимое framebuffer на одну строку текста вверх. */
    uint32_t line_bytes = g_fb->pixels_per_scanline * 4;
    uint8_t *base = (uint8_t *)(uintptr_t)g_fb->base;
    uint32_t scroll_px = FONT_HEIGHT;

    memmove(base, base + (uint64_t)scroll_px * line_bytes,
            (uint64_t)(g_fb->height - scroll_px) * line_bytes);

    uint32_t bg = pack_color(g_bg);
    for (uint32_t y = g_fb->height - scroll_px; y < g_fb->height; y++) {
        uint32_t *row = (uint32_t *)(base + (uint64_t)y * line_bytes);
        for (uint32_t x = 0; x < g_fb->width; x++) row[x] = bg;
    }

    g_row = g_rows - 1;
}

void console_putchar(char c) {
    /* Любая новая печать возвращает к живому виду — как в обычном
     * терминале: набрал что-то во время просмотра истории — тебя
     * вернуло вниз, к месту, где реально появляется новый текст. */
    if (g_scroll_offset != 0) {
        g_scroll_offset = 0;
        console_redraw();
    }

    if (c == '\n') {
        hist_new_line();
        g_col = 0;
        g_row++;
        scroll_if_needed();
        return;
    }
    if (c == '\r') {
        g_col = 0;
        return;
    }
    if (c == '\b') {
        if (g_col > 0) {
            g_col--;
            draw_glyph(g_col, g_row, ' ');
            hist_put(g_col, ' ');
        }
        return;
    }

    draw_glyph(g_col, g_row, c);
    hist_put(g_col, c);
    g_col++;
    if (g_col >= g_cols) {
        hist_new_line();
        g_col = 0;
        g_row++;
        scroll_if_needed();
    }
}

void console_print(const char *s) {
    while (*s) console_putchar(*s++);
}

void console_print_hex(uint64_t value) {
    console_print("0x");
    char buf[17];
    const char *digits = "0123456789ABCDEF";
    buf[16] = 0;
    for (int i = 0; i < 16; i++) {
        buf[15 - i] = digits[value & 0xF];
        value >>= 4;
    }
    console_print(buf);
}

void console_print_dec(uint64_t value) {
    char buf[21];
    int i = 20;
    buf[i--] = 0;
    if (value == 0) {
        buf[i--] = '0';
    } else {
        while (value > 0) {
            buf[i--] = '0' + (value % 10);
            value /= 10;
        }
    }
    console_print(&buf[i + 1]);
}
