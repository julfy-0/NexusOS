#include <stdint.h>
#include "renderer.h"
#include "gui_state.h"
#include "font.h"
#include "pit.h"
#include "mouse.h"

#define WALLPAPER_W 480
#define WALLPAPER_H 270

extern const uint8_t _binary_assets_wallpapers_nexus_default_rgb565_start[];
extern const uint8_t _binary_assets_wallpapers_nexus_default_rgb565_end[];

static uint32_t pack(uint32_t rgb) {
    nexus_gui_context_t *ctx = gui_context();
    uint8_t r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
    if (ctx->fb->pixel_format == NEXUS_PIXFMT_BGR)
        return b | ((uint32_t)g << 8) | ((uint32_t)r << 16);
    return r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

static uint32_t unpack(uint32_t pixel) {
    nexus_gui_context_t *ctx = gui_context();
    uint8_t a = pixel & 255, b = (pixel >> 8) & 255, c = (pixel >> 16) & 255;
    if (ctx->fb->pixel_format == NEXUS_PIXFMT_BGR)
        return ((uint32_t)a << 16) | ((uint32_t)b << 8) | c;
    return ((uint32_t)c << 16) | ((uint32_t)b << 8) | a;
}

void gui_renderer_px(int x, int y, uint32_t color) {
    nexus_gui_context_t *ctx = gui_context();
    if (!ctx->fb || x < 0 || y < 0 || (uint32_t)x >= ctx->fb->width || (uint32_t)y >= ctx->fb->height) return;
    uint32_t *p = (uint32_t *)(uintptr_t)(ctx->fb->base + (uint64_t)y * ctx->fb->pixels_per_scanline * 4);
    p[x] = pack(color);
}

void gui_renderer_blend_px(int x, int y, uint32_t color, uint8_t alpha) {
    nexus_gui_context_t *ctx = gui_context();
    if (!ctx->fb || x < 0 || y < 0 || (uint32_t)x >= ctx->fb->width || (uint32_t)y >= ctx->fb->height) return;
    uint32_t *p = (uint32_t *)(uintptr_t)(ctx->fb->base + (uint64_t)y * ctx->fb->pixels_per_scanline * 4);
    uint32_t old = unpack(p[x]);
    uint32_t r = (((color >> 16) & 255) * alpha + ((old >> 16) & 255) * (255 - alpha)) / 255;
    uint32_t g = (((color >> 8) & 255) * alpha + ((old >> 8) & 255) * (255 - alpha)) / 255;
    uint32_t b = ((color & 255) * alpha + (old & 255) * (255 - alpha)) / 255;
    p[x] = pack((r << 16) | (g << 8) | b);
}

void gui_renderer_rect(int x, int y, int w, int h, uint32_t color) {
    for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) gui_renderer_px(x + i, y + j, color);
}

void gui_renderer_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha) {
    for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) gui_renderer_blend_px(x + i, y + j, color, alpha);
}

void gui_renderer_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha) {
    if (r < 1) { gui_renderer_rect_alpha(x, y, w, h, color, alpha); return; }
    gui_renderer_rect_alpha(x + r, y, w - 2 * r, h, color, alpha);
    gui_renderer_rect_alpha(x, y + r, r, h - 2 * r, color, alpha);
    gui_renderer_rect_alpha(x + w - r, y + r, r, h - 2 * r, color, alpha);
    for (int yy = 0; yy < r; ++yy) for (int xx = 0; xx < r; ++xx) {
        int dx = xx - r + 1, dy = yy - r + 1;
        if (dx * dx + dy * dy <= r * r) {
            gui_renderer_blend_px(x + xx, y + yy, color, alpha);
            gui_renderer_blend_px(x + w - 1 - xx, y + yy, color, alpha);
            gui_renderer_blend_px(x + xx, y + h - 1 - yy, color, alpha);
            gui_renderer_blend_px(x + w - 1 - xx, y + h - 1 - yy, color, alpha);
        }
    }
}

void gui_renderer_circle(int cx, int cy, int r, uint32_t color, uint8_t alpha) {
    int rr = r * r;
    for (int y = -r; y <= r; ++y) for (int x = -r; x <= r; ++x)
        if (x * x + y * y <= rr) gui_renderer_blend_px(cx + x, cy + y, color, alpha);
}

void gui_renderer_text(const char *text, int x, int y, int scale, uint32_t color) {
    for (int i = 0; text[i]; ++i) {
        const uint8_t *g = gui_font_glyph(text[i]);
        if (!g) continue;
        for (int gy = 0; gy < 16; ++gy)
            for (int gx = 0; gx < 8; ++gx)
                if ((g[gy] >> (7 - gx)) & 1)
                    gui_renderer_rect(x + i * 8 * scale + gx * scale, y + gy * scale, scale, scale, color);
    }
}

int gui_renderer_text_width(const char *text, int scale) {
    int n = 0; while (text[n]) ++n; return n * 8 * scale;
}

void gui_renderer_text_center(const char *text, int y, int scale, uint32_t color) {
    nexus_gui_context_t *ctx = gui_context();
    gui_renderer_text(text, ((int)ctx->fb->width - gui_renderer_text_width(text, scale)) / 2, y, scale, color);
}

void gui_renderer_wallpaper(void) {
    nexus_gui_context_t *ctx = gui_context();
    const uint8_t *src = _binary_assets_wallpapers_nexus_default_rgb565_start;
    for (uint32_t y = 0; y < ctx->fb->height; ++y) {
        uint32_t sy = (uint64_t)y * WALLPAPER_H / ctx->fb->height;
        for (uint32_t x = 0; x < ctx->fb->width; ++x) {
            uint32_t sx = (uint64_t)x * WALLPAPER_W / ctx->fb->width;
            uint32_t off = (sy * WALLPAPER_W + sx) * 2;
            uint16_t v = (uint16_t)src[off] | ((uint16_t)src[off + 1] << 8);
            uint32_t r = ((v >> 11) & 31) * 255 / 31;
            uint32_t g = ((v >> 5) & 63) * 255 / 63;
            uint32_t b = (v & 31) * 255 / 31;
            gui_renderer_px((int)x, (int)y, (r << 16) | (g << 8) | b);
        }
    }
}

void gui_renderer_search_icon(int cx, int cy) {
    for (int y = -7; y <= 7; ++y) for (int x = -7; x <= 7; ++x) {
        int d = x * x + y * y;
        if (d >= 34 && d <= 58) gui_renderer_blend_px(cx + x, cy + y, 0xE8E8EF, 230);
    }
    for (int i = 0; i < 7; ++i) gui_renderer_blend_px(cx + 6 + i, cy + 6 + i, 0xE8E8EF, 230);
}

void gui_renderer_nexus_button(int cx, int cy) {
    gui_renderer_circle(cx, cy, 13, 0x17151F, 190);
    for (int y = -8; y <= 8; ++y) for (int x = -8; x <= 8; ++x) {
        int d = x*x + y*y;
        if (d >= 43 && d <= 64) gui_renderer_blend_px(cx + x, cy + y, 0xF1EFF6, 230);
    }
}

void gui_renderer_app_icon(int x, int y, int selected, int index) {
    uint32_t bg = selected ? 0xF0F0F3 : 0xD4D4D8;
    uint8_t alpha = selected ? 245 : 220;
    gui_renderer_rect_alpha(x, y, 30, 30, bg, alpha);
    if (selected) {
        gui_renderer_rect_alpha(x - 3, y - 3, 36, 2, 0xFFFFFF, 170);
        gui_renderer_rect_alpha(x - 3, y + 31, 36, 2, 0xFFFFFF, 170);
    }
    if (index == 0) gui_renderer_rect_alpha(x + 7, y + 8, 16, 12, 0x7A728D, 180);
    else if (index == 1) gui_renderer_text(">_", x + 5, y + 7, 1, 0x6A6178);
    else if (index == 2) {
        gui_renderer_circle(x + 15, y + 15, 7, 0x6A6178, 180);
        gui_renderer_circle(x + 15, y + 15, 3, bg, 245);
    }
}

void gui_renderer_uptime(int x, int y) {
    uint64_t s = pit_get_uptime_seconds();
    uint64_t h = (s / 3600) % 24, m = (s / 60) % 60;
    char buf[6] = {'0' + (char)(h / 10), '0' + (char)(h % 10), ':', '0' + (char)(m / 10), '0' + (char)(m % 10), 0};
    gui_renderer_text(buf, x, y, 2, 0xF3F2F7);
    gui_renderer_text("NEXUSOS", x + 2, y + 21, 1, 0xD5D0DD);
}

void gui_renderer_cursor(void) {
    nexus_gui_context_t *ctx = gui_context();
    (void)ctx;
    extern int mouse_get_x(void);
    extern int mouse_get_y(void);
    int mx = mouse_get_x(), my = mouse_get_y();
    for (int i = 0; i < 12; ++i) for (int j = 0; j <= i / 2; ++j) gui_renderer_px(mx + j, my + i, 0xFFFFFF);
    for (int i = 0; i < 12; ++i) gui_renderer_px(mx, my + i, 0x1A1622);
}
