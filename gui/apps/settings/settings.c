#include "settings.h"
#include "gui_state.h"
#include "renderer.h"
#include "nexus_version.h"

void gui_settings_open(void) {
    nexus_gui_context_t *ctx = gui_context();
    ctx->view = GUI_VIEW_SETTINGS;
    ctx->settings_item = 0;
}

void gui_settings_draw(void) {
    nexus_gui_context_t *ctx = gui_context();
    gui_renderer_wallpaper();
    int w=(int)ctx->fb->width*2/3, h=(int)ctx->fb->height*2/3;
    if (w < 520) w=(int)ctx->fb->width-24;
    if (h < 330) h=(int)ctx->fb->height-24;
    int x=((int)ctx->fb->width-w)/2, y=((int)ctx->fb->height-h)/2;
    gui_renderer_rect_alpha(x,y,w,h,0x17141F,238);
    gui_renderer_rect_alpha(x,y,w,36,0x302A3B,248);
    gui_renderer_text("NexusOS Settings",x+16,y+10,1,0xF4F1F8);
    gui_renderer_text("ESC",x+w-40,y+10,1,0xD7D1E1);
    gui_renderer_text("SYSTEM",x+20,y+58,1,0xBFAEFF);
    const char *items[5] = {
        "Version   NexusOS " NEXUS_VERSION_DISPLAY,
        "Display   Automatic framebuffer resolution",
        "Input     PS/2 Keyboard and Mouse",
        "Uptime    System runtime",
        "About     NexusOS x86_64 / UEFI"
    };
    for(int i=0;i<5;i++) {
        int iy=y+82+i*42;
        if(i==ctx->settings_item) gui_renderer_rect_alpha(x+14,iy-6,w-28,32,0x5A506B,170);
        gui_renderer_text(items[i],x+28,iy,1,i==ctx->settings_item?0xFFFFFF:0xD8D3E0);
    }
    gui_renderer_text("Up/Down: select   Enter: info   Esc: desktop",x+20,y+h-28,1,0xAFA8BC);
}

void gui_settings_select(void) {
    gui_context()->message = "Settings item selected - editing comes in a future update";
}
