#include "search.h"
#include "gui_state.h"
#include "renderer.h"
#include "terminal.h"
#include "files.h"
#include "settings.h"

static const char *g_search_names[GUI_SEARCH_COUNT] = {"Files", "Terminal", "Settings", "Reboot", "Shutdown"};

static int search_match(const char *name) {
    nexus_gui_context_t *ctx = gui_context();
    if (!ctx->search_len) return 1;
    for (int i=0; name[i]; ++i) {
        int j=0;
        while (ctx->search_query[j] && name[i+j]) {
            char a=name[i+j], b=ctx->search_query[j];
            if (a>='A' && a<='Z') a+=32;
            if (b>='A' && b<='Z') b+=32;
            if (a!=b) break;
            ++j;
        }
        if (!ctx->search_query[j]) return 1;
    }
    return 0;
}

int gui_search_visible_count(void) {
    int n=0; for(int i=0;i<GUI_SEARCH_COUNT;i++) if(search_match(g_search_names[i])) n++;
    return n;
}

static int search_result_at(int selected) {
    int n=0;
    for(int i=0;i<GUI_SEARCH_COUNT;i++) if(search_match(g_search_names[i])) if(n++==selected) return i;
    return -1;
}

void gui_search_open(void) {
    nexus_gui_context_t *ctx = gui_context();
    ctx->view=GUI_VIEW_SEARCH;
    ctx->search_len=0; ctx->search_query[0]=0; ctx->search_selected=0;
}

void gui_search_draw(void) {
    nexus_gui_context_t *ctx=gui_context();
    gui_renderer_wallpaper();
    int w=(int)ctx->fb->width*2/3, h=(int)ctx->fb->height*2/3;
    if(w<500) w=(int)ctx->fb->width-24;
    if(h<280) h=(int)ctx->fb->height-24;
    int x=((int)ctx->fb->width-w)/2, y=((int)ctx->fb->height-h)/2;
    gui_renderer_rect_alpha(x,y,w,h,0x17141F,242); gui_renderer_rect_alpha(x,y,w,42,0x302A3B,250);
    gui_renderer_text("NexusOS Search",x+16,y+13,1,0xF4F1F8); gui_renderer_text("ESC",x+w-40,y+13,1,0xD7D1E1);
    gui_renderer_rect_alpha(x+18,y+58,w-36,34,0x383240,235); gui_renderer_search_icon(x+36,y+75);
    gui_renderer_text("> ",x+54,y+68,1,0xFFFFFF); gui_renderer_text(ctx->search_query,x+70,y+68,1,0xFFFFFF);
    gui_renderer_text("_",x+70+ctx->search_len*8,y+68,1,0xBFAEFF);
    gui_renderer_text("Applications & System",x+20,y+116,1,0xBFAEFF);
    int row=0;
    for(int i=0;i<GUI_SEARCH_COUNT;i++) if(search_match(g_search_names[i])) {
        int iy=y+144+row*34;
        if(row==ctx->search_selected) gui_renderer_rect_alpha(x+16,iy-5,w-32,28,0x5A506B,180);
        gui_renderer_text(g_search_names[i],x+32,iy,1,row==ctx->search_selected?0xFFFFFF:0xD8D3E0); row++;
    }
    if(!row) gui_renderer_text("No results",x+32,y+148,1,0xAFA8BC);
    gui_renderer_text("Type to search  Up/Down: select  Enter: open  Esc: desktop",x+20,y+h-28,1,0xAFA8BC);
}

void gui_search_execute(void) {
    nexus_gui_context_t *ctx=gui_context();
    int r=search_result_at(ctx->search_selected);
    if(r==0) gui_files_open();
    else if(r==1) gui_terminal_open();
    else if(r==2) gui_settings_open();
    else if(r==3) { ctx->message="Reboot is available from NexusOS Command Line"; ctx->view=GUI_VIEW_DESKTOP; }
    else if(r==4) { ctx->message="Shutdown is available from NexusOS Command Line"; ctx->view=GUI_VIEW_DESKTOP; }
}
