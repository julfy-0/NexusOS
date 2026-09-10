#include "files.h"
#include "gui_state.h"
#include "renderer.h"
#include "vfs.h"

void gui_files_open(void) {
    nexus_gui_context_t *ctx = gui_context();
    ctx->view = GUI_VIEW_FILES;
    ctx->files_item = 0;
}

void gui_files_draw(void) {
    nexus_gui_context_t *ctx = gui_context();
    char names[24][VFS_NAME_LEN]; unsigned char dirs[24]; char cwd[128];
    int count = vfs_gui_list(names, dirs, 24); vfs_gui_getcwd(cwd, sizeof(cwd));
    gui_renderer_wallpaper();
    int w=(int)ctx->fb->width*3/4, h=(int)ctx->fb->height*3/4;
    if(w<520) w=(int)ctx->fb->width-24;
    if(h<340) h=(int)ctx->fb->height-24;
    int x=((int)ctx->fb->width-w)/2, y=((int)ctx->fb->height-h)/2;
    gui_renderer_rect_alpha(x,y,w,h,0x17141F,238); gui_renderer_rect_alpha(x,y,w,36,0x302A3B,248);
    gui_renderer_text("NexusOS Files",x+16,y+10,1,0xF4F1F8); gui_renderer_text("ESC",x+w-40,y+10,1,0xD7D1E1);
    gui_renderer_text(cwd,x+18,y+52,1,0xBFAEFF);
    gui_renderer_text("..",x+28,y+82,1,0xD8D3E0);
    if(ctx->files_item==0) gui_renderer_rect_alpha(x+14,y+76,w-28,24,0x5A506B,170);
    for(int i=0;i<count && i<12;i++) {
        int iy=y+108+i*28, sel=i+1==ctx->files_item;
        if(sel) gui_renderer_rect_alpha(x+14,iy-4,w-28,24,0x5A506B,170);
        gui_renderer_text(dirs[i]?"[DIR]":"[FILE]",x+28,iy,1,dirs[i]?0xC9B9FF:0xBFC3D0);
        gui_renderer_text(names[i],x+90,iy,1,sel?0xFFFFFF:0xD8D3E0);
    }
    if(!count) gui_renderer_text("(empty)",x+28,y+112,1,0xAFA8BC);
    gui_renderer_text("Up/Down: select  Enter: open  Backspace: up  Esc: desktop",x+18,y+h-28,1,0xAFA8BC);
}

void gui_files_open_selected(void) {
    nexus_gui_context_t *ctx = gui_context();
    char names[24][VFS_NAME_LEN]; unsigned char dirs[24];
    int count=vfs_gui_list(names,dirs,24);
    if(ctx->files_item==0) { vfs_cd(".."); ctx->files_item=0; }
    else { int i=ctx->files_item-1; if(i<count && dirs[i]) { vfs_cd(names[i]); ctx->files_item=0; } }
}
