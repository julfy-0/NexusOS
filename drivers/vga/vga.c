/*
 * NexusOS — драйвер текстового режима VGA (80x25, память 0xB8000).
 * Каждая ячейка экрана — 2 байта: символ + байт атрибута (цвет).
 */
#include "vga.h"
#include <lib/string.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((u16 *)0xB8000)

static u16 *const vga_buffer = VGA_MEMORY;
static size_t vga_row;
static size_t vga_col;
static u8     vga_color;

static inline u16 vga_entry(char c, u8 color) {
    return (u16)c | ((u16)color << 8);
}

void vga_set_color(enum vga_color fg, enum vga_color bg) {
    vga_color = (u8)fg | (u8)(bg << 4);
}

void vga_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', vga_color);
        }
    }
    vga_row = 0;
    vga_col = 0;
}

void vga_init(void) {
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

static void vga_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        memcpy(&vga_buffer[(y - 1) * VGA_WIDTH],
               &vga_buffer[y * VGA_WIDTH],
               VGA_WIDTH * sizeof(u16));
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', vga_color);
    }
    vga_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\t') {
        vga_col = (vga_col + 4) & ~3;
    } else {
        vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(c, vga_color);
        vga_col++;
    }

    if (vga_col >= VGA_WIDTH) {
        vga_col = 0;
        vga_row++;
    }
    if (vga_row >= VGA_HEIGHT) {
        vga_scroll();
    }
}

void vga_puts(const char *s) {
    while (*s) {
        vga_putchar(*s++);
    }
}
