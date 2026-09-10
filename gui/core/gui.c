#include "gui.h"
#include "gui_state.h"
#include "desktop.h"
#include "terminal.h"
#include "files.h"
#include "settings.h"
#include "search.h"
#include "input.h"
#include "mouse.h"

void gui_init(nexus_framebuffer_t *fb) {
    nexus_gui_context_t *ctx=gui_context();
    ctx->fb=fb;
    if(fb)mouse_set_screen_size(fb->width,fb->height);
    ctx->active=0;ctx->selected_app=0;ctx->view=GUI_VIEW_DESKTOP;
    ctx->message="Left/Right: select  Enter: open  Esc: return";
}

void gui_start(void) {
    nexus_gui_context_t *ctx=gui_context();
    if(!ctx->fb)return;
    ctx->active=1;ctx->selected_app=0;ctx->view=GUI_VIEW_DESKTOP;
    ctx->last_clock_second=(uint64_t)-1;ctx->prev_buttons=mouse_get_buttons();
    ctx->search_len=0;ctx->search_query[0]=0;ctx->search_selected=0;
    gui_draw_desktop();
}

void gui_exit(void) { nexus_gui_context_t *ctx=gui_context();ctx->active=0;ctx->view=GUI_VIEW_DESKTOP; }
int gui_is_active(void) { return gui_context()->active; }
int gui_handle_key(int key) { return gui_input_handle_key(key); }

void gui_status(const char *text) {
    nexus_gui_context_t *ctx=gui_context();ctx->message=text?text:"";
    if(ctx->active)gui_draw_desktop();
}

void gui_draw_desktop(void) {
    nexus_gui_context_t *ctx=gui_context();
    if(!ctx->fb)return;
    if(ctx->view==GUI_VIEW_TERMINAL){gui_terminal_draw();return;}
    if(ctx->view==GUI_VIEW_SEARCH){gui_search_draw();return;}
    if(ctx->view==GUI_VIEW_FILES){gui_files_draw();return;}
    if(ctx->view==GUI_VIEW_SETTINGS){gui_settings_draw();return;}
    gui_desktop_draw();
}

void gui_update(void) { gui_input_update(); }
