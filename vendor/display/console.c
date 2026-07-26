/* NexusOS: простейшая текстовая консоль поверх линейного framebuffer.
 * Никакого аппаратного ускорения — просто пишем пиксели напрямую. */
#include "console.h"
#include "font8x16.h"

extern void *memmove(void *dest, const void *src, unsigned long n);
extern void *memset(void *dest, int value, unsigned long n);

static nexus_framebuffer_t *g_fb;
static uint32_t g_fg = 0xFFFFFF;
static uint32_t g_bg = 0x000000;
static uint32_t g_col = 0;
static uint32_t g_row = 0;
static uint32_t g_cols;
static uint32_t g_rows;

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
    console_clear();
}

void console_clear(void) {
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

static void draw_glyph(uint32_t col, uint32_t row, char c) {
    uint32_t fg = pack_color(g_fg);
    uint32_t bg = pack_color(g_bg);

    const uint8_t *glyph;
    if (c >= FONT_FIRST_CHAR && c <= FONT_LAST_CHAR) {
        glyph = font8x16[(uint8_t)c - FONT_FIRST_CHAR];
    } else {
        glyph = font8x16[0]; /* неизвестный символ -> пробел */
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
    if (c == '\n') {
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
        }
        return;
    }

    draw_glyph(g_col, g_row, c);
    g_col++;
    if (g_col >= g_cols) {
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
