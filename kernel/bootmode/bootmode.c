/* NexusOS graphical boot mode selector.
 *
 * The selector runs after ExitBootServices(), so it uses the kernel's GOP
 * framebuffer directly and does not depend on UEFI console services.
 * Keyboard: Up/Down + Enter. Mouse: hover + left click.
 */
#include "bootmode.h"
#include "keyboard.h"
#include "mouse.h"
#include "font8x16.h"
#include "io.h"
#include <stdint.h>

#define KBD_DATA   0x60
#define KBD_STATUS 0x64
#define KBD_OBF    0x01
#define KBD_AUX    0x20

#define SC_EXT     0xE0
#define SC_UP      0x48
#define SC_DOWN    0x50
#define SC_ENTER   0x1C

#define MENU_COUNT 3

typedef struct {
    nexus_framebuffer_t *fb;
    uint32_t width;
    uint32_t height;
} menu_ctx_t;

static menu_ctx_t g_menu;

static uint32_t pack_rgb(uint32_t rgb) {
    uint8_t r = (uint8_t)(rgb >> 16);
    uint8_t g = (uint8_t)(rgb >> 8);
    uint8_t b = (uint8_t)rgb;
    if (g_menu.fb->pixel_format == NEXUS_PIXFMT_BGR)
        return ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void px(int x, int y, uint32_t rgb) {
    if (!g_menu.fb) return;
    if (x < 0 || y < 0 || (uint32_t)x >= g_menu.width || (uint32_t)y >= g_menu.height) return;
    volatile uint32_t *base = (volatile uint32_t *)(uintptr_t)g_menu.fb->base;
    base[(uint64_t)(uint32_t)y * g_menu.fb->pixels_per_scanline + (uint32_t)x] = pack_rgb(rgb);
}

static void rect(int x, int y, int w, int h, uint32_t rgb) {
    if (w <= 0 || h <= 0) return;
    for (int yy = 0; yy < h; ++yy)
        for (int xx = 0; xx < w; ++xx)
            px(x + xx, y + yy, rgb);
}

static void hline(int x0, int x1, int y, uint32_t rgb) {
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    for (int x = x0; x <= x1; ++x) px(x, y, rgb);
}

static void vline(int x, int y0, int y1, uint32_t rgb) {
    if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; ++y) px(x, y, rgb);
}

static void rounded_rect(int x, int y, int w, int h, int radius, uint32_t rgb) {
    if (w <= 0 || h <= 0) return;
    if (radius < 1) { rect(x, y, w, h, rgb); return; }
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;

    rect(x + radius, y, w - radius * 2, h, rgb);
    rect(x, y + radius, radius, h - radius * 2, rgb);
    rect(x + w - radius, y + radius, radius, h - radius * 2, rgb);

    int r2 = radius * radius;
    for (int dy = 0; dy < radius; ++dy) {
        for (int dx = 0; dx < radius; ++dx) {
            int ox = radius - 1 - dx;
            int oy = radius - 1 - dy;
            if (ox * ox + oy * oy <= r2) {
                px(x + dx, y + dy, rgb);
                px(x + w - 1 - dx, y + dy, rgb);
                px(x + dx, y + h - 1 - dy, rgb);
                px(x + w - 1 - dx, y + h - 1 - dy, rgb);
            }
        }
    }
}

static uint32_t lerp_channel(uint32_t a, uint32_t b, uint32_t t) {
    return (a * (255u - t) + b * t) / 255u;
}

static uint32_t lerp_rgb(uint32_t top, uint32_t bottom, uint32_t t) {
    uint32_t r = lerp_channel((top >> 16) & 0xFF, (bottom >> 16) & 0xFF, t);
    uint32_t g = lerp_channel((top >> 8) & 0xFF, (bottom >> 8) & 0xFF, t);
    uint32_t b = lerp_channel(top & 0xFF, bottom & 0xFF, t);
    return (r << 16) | (g << 8) | b;
}

static void glyph_big(int x, int y, char c, int scale, uint32_t top, uint32_t bottom) {
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) return;
    const uint8_t *g = font8x16[(uint8_t)c - FONT_FIRST_CHAR];
    for (int gy = 0; gy < 16; ++gy) {
        uint32_t t = (uint32_t)(gy * 255 / 15);
        uint32_t col = lerp_rgb(top, bottom, t);
        for (int gx = 0; gx < 8; ++gx) {
            if ((g[gy] >> (7 - gx)) & 1u)
                rect(x + gx * scale, y + gy * scale, scale, scale, col);
        }
    }
}

static int text_width(const char *s, int scale, int spacing) {
    int n = 0;
    while (s[n]) ++n;
    if (n == 0) return 0;
    return n * 8 * scale + (n - 1) * spacing;
}

static void draw_text_center(const char *s, int y, int scale, int spacing,
                             uint32_t top, uint32_t bottom) {
    int w = text_width(s, scale, spacing);
    int x = ((int)g_menu.width - w) / 2;
    for (int i = 0; s[i]; ++i) {
        glyph_big(x, y, s[i], scale, top, bottom);
        x += 8 * scale + spacing;
    }
}

static void draw_text_at(const char *s, int x, int y, int scale, int spacing,
                         uint32_t color) {
    for (int i = 0; s[i]; ++i) {
        glyph_big(x, y, s[i], scale, color, color);
        x += 8 * scale + spacing;
    }
}

/* Geometric N matching the NexusOS boot branding. */
static void draw_logo(int cx, int cy, int size) {
    int w = size;
    int h = size;
    int x = cx - w / 2;
    int y = cy - h / 2;
    int t = w / 5;
    if (t < 8) t = 8;

    /* Left pillar. */
    for (int yy = 0; yy < h; ++yy) {
        uint32_t tcol = (uint32_t)(yy * 255 / (h ? h : 1));
        uint32_t c = lerp_rgb(0xFFFFFF, 0x737373, tcol);
        int dx = yy * (w - t) / h;
        if (dx < w - t) {
            rect(x, y + yy, t, 1, c);
            int diag_x = x + dx;
            if (diag_x > x + t) rect(diag_x, y + yy, t, 1, c);
        }
    }

    /* Right pillar and cleanup to make the mark sharp. */
    for (int yy = 0; yy < h; ++yy) {
        uint32_t tcol = (uint32_t)(yy * 255 / (h ? h : 1));
        uint32_t c = lerp_rgb(0xFFFFFF, 0x707070, tcol);
        rect(x + w - t, y + yy, t, 1, c);
    }
    /* Cut the center to produce the characteristic split N. */
    for (int yy = h / 2 - t / 2; yy < h / 2 + t / 2; ++yy)
        rect(x + t, y + yy, w - t * 2, 1, 0x000000);

    /* Re-add the diagonal ribbon with a brighter upper half. */
    for (int yy = 0; yy < h; ++yy) {
        int sx = x + t + yy * (w - 2 * t) / h;
        uint32_t c = lerp_rgb(0xFFFFFF, 0x7A7A7A, (uint32_t)(yy * 255 / (h ? h : 1)));
        rect(sx, y + yy, t, 1, c);
    }
}

static int action_from_keyboard(void) {
    static int extended = 0;
    if (!(inb(KBD_STATUS) & KBD_OBF)) return 0;
    uint8_t status = inb(KBD_STATUS);
    if (status & KBD_AUX) return 0;
    uint8_t sc = inb(KBD_DATA);
    if (sc == SC_EXT) { extended = 1; return 0; }
    if (extended) {
        extended = 0;
        if (!(sc & 0x80)) {
            if (sc == SC_UP) return -1;
            if (sc == SC_DOWN) return 1;
        }
        return 0;
    }
    if (sc == SC_ENTER) return 2;
    return 0;
}

static void hard_reboot(void) {
    /* Reset through the legacy i8042 controller. */
    for (uint32_t i = 0; i < 100000; ++i) {
        if (!(inb(0x64) & 0x02)) break;
    }
    outb(0x64, 0xFE);
    for (;;) __asm__ volatile("cli; hlt");
}

static int button_hit(int index, int32_t mx, int32_t my,
                      int x, int y, int w, int h) {
    (void)index;
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

static void draw_menu(int selected, int hover) {
    rect(0, 0, (int)g_menu.width, (int)g_menu.height, 0x000000);

    int min_side = (int)(g_menu.width < g_menu.height ? g_menu.width : g_menu.height);
    int logo_size = min_side / 5;
    if (logo_size < 150) logo_size = 150;
    if (logo_size > 250) logo_size = 250;
    int logo_cy = (int)g_menu.height / 2 - min_side / 5;
    draw_logo((int)g_menu.width / 2, logo_cy, logo_size);

    int bw = (int)((g_menu.width * 60u) / 100u);
    if (bw < 650) bw = 650;
    if (bw > 1100) bw = 1100;
    if (bw > (int)g_menu.width - 80) bw = (int)g_menu.width - 80;
    int bh = (int)(g_menu.height / 12);
    if (bh < 62) bh = 62;
    if (bh > 92) bh = 92;
    int gap = bh / 2;
    int total_h = MENU_COUNT * bh + (MENU_COUNT - 1) * gap;
    int x = ((int)g_menu.width - bw) / 2;
    int y = (int)g_menu.height / 2 + min_side / 12;

    const char *labels[MENU_COUNT] = {"Graphic", "Command line", "UEFI boot"};
    for (int i = 0; i < MENU_COUNT; ++i) {
        int active = (i == selected || i == hover);
        uint32_t fill = active ? 0x2E2E2E : 0x151515;
        rounded_rect(x, y + i * (bh + gap), bw, bh, 18, fill);
        if (active) {
            rounded_rect(x + 2, y + i * (bh + gap) + 2, bw - 4, bh - 4, 16, fill);
        }
        int scale = bh >= 80 ? 2 : 1;
        int tw = text_width(labels[i], scale, scale >= 2 ? 2 : 1);
        int tx = x + (bw - tw) / 2;
        int ty = y + i * (bh + gap) + (bh - 16 * scale) / 2;
        draw_text_at(labels[i], tx, ty, scale, scale >= 2 ? 2 : 1,
                     active ? 0xFFFFFF : 0xE4E4E4);
    }

    /* Left-side keyboard hint, matching the reference composition. */
    int hx = 18;
    int hy = 22;
    const char *hints[3] = {"Up", "Down", "Run"};
    const char *keys[3] = {"^", "v", "->"};
    for (int i = 0; i < 3; ++i) {
        rounded_rect(hx, hy + i * 48, 44, 38, 12, 0x555555);
        draw_text_at(keys[i], hx + 12, hy + i * 48 + 8, 1, 1, 0xFFFFFF);
        draw_text_at(hints[i], hx + 60, hy + i * 48 + 8, 1, 1, 0xF0F0F0);
    }

    (void)total_h;
}

nexus_boot_mode_t bootmode_select(nexus_framebuffer_t *fb) {
    if (!fb || !fb->base || !fb->width || !fb->height) return NEXUS_BOOT_CLI;
    g_menu.fb = fb;
    g_menu.width = fb->width;
    g_menu.height = fb->height;

    mouse_set_screen_size(fb->width, fb->height);

    int selected = 0;
    int hover = -1;
    uint8_t last_buttons = mouse_get_buttons();

    draw_menu(selected, hover);

    for (;;) {
        int key_action = action_from_keyboard();
        (void)mouse_poll();

        int min_side = (int)(g_menu.width < g_menu.height ? g_menu.width : g_menu.height);
        int bw = (int)((g_menu.width * 60u) / 100u);
        if (bw < 650) bw = 650;
        if (bw > 1100) bw = 1100;
        if (bw > (int)g_menu.width - 80) bw = (int)g_menu.width - 80;
        int bh = (int)(g_menu.height / 12);
        if (bh < 62) bh = 62;
        if (bh > 92) bh = 92;
        int gap = bh / 2;
        int x = ((int)g_menu.width - bw) / 2;
        int y = (int)g_menu.height / 2 + min_side / 12;

        int new_hover = -1;
        if (mouse_is_present()) {
            int32_t mx = mouse_get_x();
            int32_t my = mouse_get_y();
            for (int i = 0; i < MENU_COUNT; ++i) {
                if (button_hit(i, mx, my, x, y + i * (bh + gap), bw, bh)) {
                    new_hover = i;
                    break;
                }
            }
            if (new_hover != hover) {
                hover = new_hover;
                draw_menu(selected, hover);
            }

            uint8_t buttons = mouse_get_buttons();
            if ((buttons & 1u) && !(last_buttons & 1u) && hover >= 0) {
                if (hover == 0) return NEXUS_BOOT_GRAPHIC;
                if (hover == 1) return NEXUS_BOOT_CLI;
                hard_reboot();
            }
            last_buttons = buttons;
        }

        if (key_action == -1) {
            selected = (selected + MENU_COUNT - 1) % MENU_COUNT;
            draw_menu(selected, hover);
        } else if (key_action == 1) {
            selected = (selected + 1) % MENU_COUNT;
            draw_menu(selected, hover);
        } else if (key_action == 2) {
            if (selected == 0) return NEXUS_BOOT_GRAPHIC;
            if (selected == 1) return NEXUS_BOOT_CLI;
            hard_reboot();
        }

        /* Keep the menu responsive even before IRQs are enabled. */
        for (volatile int spin = 0; spin < 20000; ++spin) __asm__ volatile("pause");
    }
}
