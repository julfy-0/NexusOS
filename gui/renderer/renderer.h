#ifndef NEXUSOS_GUI_RENDERER_H
#define NEXUSOS_GUI_RENDERER_H

#include <stdint.h>

void gui_renderer_px(int x, int y, uint32_t color);
void gui_renderer_blend_px(int x, int y, uint32_t color, uint8_t alpha);
void gui_renderer_rect(int x, int y, int w, int h, uint32_t color);
void gui_renderer_rect_alpha(int x, int y, int w, int h, uint32_t color, uint8_t alpha);
void gui_renderer_rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t color, uint8_t alpha);
void gui_renderer_circle(int cx, int cy, int r, uint32_t color, uint8_t alpha);
void gui_renderer_text(const char *text, int x, int y, int scale, uint32_t color);
int gui_renderer_text_width(const char *text, int scale);
void gui_renderer_text_center(const char *text, int y, int scale, uint32_t color);
void gui_renderer_wallpaper(void);
void gui_renderer_search_icon(int cx, int cy);
void gui_renderer_nexus_button(int cx, int cy);
void gui_renderer_app_icon(int x, int y, int selected, int index);
void gui_renderer_uptime(int x, int y);
void gui_renderer_cursor(void);

#endif
