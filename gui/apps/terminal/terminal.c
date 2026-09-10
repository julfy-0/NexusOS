#include "terminal.h"
#include "gui_state.h"
#include "renderer.h"
#include "nexus_version.h"

static int streq(const char *a, const char *b) {
    int i = 0; while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == '\0' && b[i] == '\0';
}

void gui_terminal_open(void) {
    nexus_gui_context_t *ctx = gui_context();
    ctx->view = GUI_VIEW_TERMINAL;
    ctx->term_len = 0;
    ctx->term_line[0] = 0;
    ctx->term_output = "Welcome to NexusOS Terminal";
}

void gui_terminal_draw(void) {
    nexus_gui_context_t *ctx = gui_context();
    gui_renderer_wallpaper();
    int w=(int)ctx->fb->width*3/4, h=(int)ctx->fb->height*3/4;
    if (w < 500) w = (int)ctx->fb->width - 24;
    if (h < 300) h = (int)ctx->fb->height - 24;
    int x=((int)ctx->fb->width-w)/2, y=((int)ctx->fb->height-h)/2;
    gui_renderer_rect_alpha(x,y,w,h,0x17141F,235);
    gui_renderer_rect_alpha(x,y,w,32,0x302A3B,245);
    gui_renderer_text("NexusOS Terminal",x+14,y+8,1,0xF4F1F8);
    gui_renderer_text("ESC",x+w-38,y+8,1,0xD7D1E1);
    gui_renderer_text("NexusOS " NEXUS_VERSION_STRING " Terminal",x+16,y+52,1,0xCFC8DC);
    gui_renderer_text(ctx->term_output,x+16,y+78,1,0xEEEAF4);
    gui_renderer_text("Commands: help  clear  version  uptime  desktop",x+16,y+h-64,1,0xAFA8BC);
    gui_renderer_text("NexusOS> ",x+16,y+h-36,1,0xFFFFFF);
    gui_renderer_text(ctx->term_line,x+16+9*8,y+h-36,1,0xFFFFFF);
    gui_renderer_text("_",x+16+(9+ctx->term_len)*8,y+h-36,1,0xBFAEFF);
}

void gui_terminal_execute(void) {
    nexus_gui_context_t *ctx = gui_context();
    ctx->term_line[ctx->term_len] = 0;
    if (streq(ctx->term_line,"help")) ctx->term_output="help: clear version uptime desktop";
    else if (streq(ctx->term_line,"clear")) ctx->term_output="";
    else if (streq(ctx->term_line,"version")) ctx->term_output="NexusOS " NEXUS_VERSION_DISPLAY;
    else if (streq(ctx->term_line,"uptime")) ctx->term_output="System uptime available in Desktop panel";
    else if (streq(ctx->term_line,"desktop")) { ctx->view=GUI_VIEW_DESKTOP; ctx->term_len=0; ctx->term_line[0]=0; return; }
    else if (ctx->term_len) ctx->term_output="Unknown command. Type help";
    ctx->term_len=0; ctx->term_line[0]=0;
}
