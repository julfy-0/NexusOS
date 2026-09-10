#include "desktop.h"
#include "gui_state.h"
#include "renderer.h"
#include "nexus_version.h"

static const char *app_label(int index) {
    static const char *labels[GUI_APP_COUNT] = {
        "Files", "Terminal", "Settings", "App 4", "App 5",
        "App 6", "App 7", "App 8", "App 9", "App 10"
    };
    return (index >= 0 && index < GUI_APP_COUNT) ? labels[index] : "";
}

void gui_desktop_draw(void) {
    nexus_gui_context_t *ctx = gui_context();
    if (!ctx->fb) return;
    gui_renderer_wallpaper();

    int panel_h = ctx->fb->height >= 720 ? 58 : 48;
    int margin = ctx->fb->width >= 900 ? 22 : 10;
    int panel_y = (int)ctx->fb->height - panel_h - margin;
    int panel_w = (int)ctx->fb->width - margin * 2;
    gui_renderer_rounded_rect_alpha(margin + 2, panel_y + 3, panel_w, panel_h, 14, 0x000000, 80);
    gui_renderer_rounded_rect_alpha(margin, panel_y, panel_w, panel_h, 14, 0x17141F, 218);
    gui_renderer_rect_alpha(margin + 14, panel_y, panel_w - 28, 1, 0xD6CCEB, 75);

    int cy = panel_y + panel_h / 2;
    gui_renderer_nexus_button(margin + 30, cy);
    gui_renderer_rounded_rect_alpha(margin + 58, cy - 16, 96, 32, 10, 0x3B3547, 185);
    gui_renderer_search_icon(margin + 132, cy);
    gui_renderer_text("Search", margin + 68, cy - 5, 1, 0xD9D4E2);

    int icon_size = 32, gap = 8;
    int total = GUI_APP_COUNT * icon_size + (GUI_APP_COUNT - 1) * gap;
    int start = ((int)ctx->fb->width - total) / 2;
    for (int i = 0; i < GUI_APP_COUNT; ++i)
        gui_renderer_app_icon(start + i * (icon_size + gap), cy - icon_size / 2, i == ctx->selected_app, i);

    if (ctx->selected_app >= 0 && ctx->selected_app < 3)
        gui_renderer_text_center(app_label(ctx->selected_app), panel_y - 34, 1, 0xF3EFF8);

    int clock_w = 98;
    int clock_x = margin + panel_w - clock_w - 12;
    gui_renderer_rounded_rect_alpha(clock_x - 8, cy - 20, clock_w + 8, 40, 10, 0x282331, 145);
    gui_renderer_uptime(clock_x, cy - 18);

    if (ctx->message && ctx->message[0])
        gui_renderer_text_center(ctx->message, panel_y - 18, 1, 0xD7D1E1);
    gui_renderer_cursor();
}
