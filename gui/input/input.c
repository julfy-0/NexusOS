#include <stdint.h>
#include "input.h"
#include "gui_state.h"
#include "gui.h"
#include "desktop.h"
#include "files.h"
#include "terminal.h"
#include "settings.h"
#include "search.h"
#include "mouse.h"
#include "pit.h"
#include "vfs.h"

static void redraw(void) { gui_draw_desktop(); }

int gui_input_handle_key(int key) {
    nexus_gui_context_t *ctx=gui_context();
    if(!ctx->active) return 0;
    if(ctx->view==GUI_VIEW_SEARCH) {
        int count=gui_search_visible_count();
        if(key==GUI_KEY_ESCAPE){ctx->view=GUI_VIEW_DESKTOP;redraw();return 1;}
        if(key==GUI_KEY_UP||key=='w'||key=='W'){if(count)ctx->search_selected=(ctx->search_selected+count-1)%count;redraw();return 1;}
        if(key==GUI_KEY_DOWN||key=='s'||key=='S'){if(count)ctx->search_selected=(ctx->search_selected+1)%count;redraw();return 1;}
        if(key==GUI_KEY_ENTER){if(count)gui_search_execute();redraw();return 1;}
        if(key=='\b'){if(ctx->search_len)ctx->search_query[--ctx->search_len]=0;ctx->search_selected=0;redraw();return 1;}
        if(key>=32&&key<127&&ctx->search_len<(int)sizeof(ctx->search_query)-1){ctx->search_query[ctx->search_len++]=(char)key;ctx->search_query[ctx->search_len]=0;ctx->search_selected=0;redraw();return 1;}
        return 1;
    }
    if(ctx->view==GUI_VIEW_FILES) {
        char names[24][VFS_NAME_LEN]; unsigned char dirs[24]; int count=vfs_gui_list(names,dirs,24); int total=count+1;
        if(key==GUI_KEY_ESCAPE){ctx->view=GUI_VIEW_DESKTOP;redraw();return 1;}
        if(key==GUI_KEY_UP||key=='w'||key=='W'){ctx->files_item=(ctx->files_item+total-1)%total;redraw();return 1;}
        if(key==GUI_KEY_DOWN||key=='s'||key=='S'){ctx->files_item=(ctx->files_item+1)%total;redraw();return 1;}
        if(key==GUI_KEY_ENTER){gui_files_open_selected();redraw();return 1;}
        if(key=='\b'){vfs_cd("..");ctx->files_item=0;redraw();return 1;}
        return 1;
    }
    if(ctx->view==GUI_VIEW_SETTINGS) {
        if(key==GUI_KEY_ESCAPE){ctx->view=GUI_VIEW_DESKTOP;redraw();return 1;}
        if(key==GUI_KEY_UP||key=='w'||key=='W'){ctx->settings_item=(ctx->settings_item+4)%5;redraw();return 1;}
        if(key==GUI_KEY_DOWN||key=='s'||key=='S'){ctx->settings_item=(ctx->settings_item+1)%5;redraw();return 1;}
        if(key==GUI_KEY_ENTER){gui_settings_select();redraw();return 1;}
        return 1;
    }
    if(ctx->view==GUI_VIEW_TERMINAL) {
        if(key==GUI_KEY_ESCAPE){ctx->view=GUI_VIEW_DESKTOP;redraw();return 1;}
        if(key==GUI_KEY_ENTER||key=='\n'){gui_terminal_execute();redraw();return 1;}
        if(key=='\b'){if(ctx->term_len)ctx->term_line[--ctx->term_len]=0;redraw();return 1;}
        if(key>=32&&key<127&&ctx->term_len<(int)sizeof(ctx->term_line)-1){ctx->term_line[ctx->term_len++]=(char)key;ctx->term_line[ctx->term_len]=0;redraw();}
        return 1;
    }
    if(key==GUI_KEY_ESCAPE){ctx->active=0;ctx->view=GUI_VIEW_DESKTOP;return 1;}
    if(key=='/'||key=='q'||key=='Q'){gui_search_open();redraw();return 1;}
    if(key==GUI_KEY_LEFT||key=='a'||key=='A'){ctx->selected_app=(ctx->selected_app+GUI_APP_COUNT-1)%GUI_APP_COUNT;redraw();return 1;}
    if(key==GUI_KEY_RIGHT||key=='d'||key=='D'){ctx->selected_app=(ctx->selected_app+1)%GUI_APP_COUNT;redraw();return 1;}
    if(key==GUI_KEY_UP||key==GUI_KEY_DOWN||key=='w'||key=='W'||key=='s'||key=='S')return 1;
    if(key==GUI_KEY_ENTER){
        if(ctx->selected_app==0)gui_files_open();
        else if(ctx->selected_app==1)gui_terminal_open();
        else if(ctx->selected_app==2)gui_settings_open();
        else ctx->message="Application slot reserved for NexusOS 0.5.1";
        redraw(); return 1;
    }
    return 1;
}

void gui_input_update(void) {
    nexus_gui_context_t *ctx=gui_context();
    if(!ctx->active||!ctx->fb)return;
    uint64_t now=pit_get_uptime_seconds();
    int redraw_needed=0;
    int moved=mouse_has_moved();
    if(moved){mouse_clear_moved();redraw_needed=1;}
    int mx=mouse_get_x(), my=mouse_get_y();
    uint8_t buttons=mouse_get_buttons(), pressed=buttons & (uint8_t)~ctx->prev_buttons;
    ctx->prev_buttons=buttons;

    if(ctx->view==GUI_VIEW_SEARCH){
        int w=(int)ctx->fb->width*2/3,h=(int)ctx->fb->height*2/3;if(w<500)w=(int)ctx->fb->width-24;if(h<280)h=(int)ctx->fb->height-24;
        int x=((int)ctx->fb->width-w)/2,y=((int)ctx->fb->height-h)/2,count=gui_search_visible_count();
        if(moved&&mx>=x+16&&mx<x+w-16&&my>=y+139&&my<y+144+count*34){int row=(my-(y+139))/34;if(row>=0&&row<count&&row!=ctx->search_selected){ctx->search_selected=row;redraw_needed=1;}}
        if((pressed&1)&&mx>=x+w-60&&my>=y&&my<y+42){ctx->view=GUI_VIEW_DESKTOP;redraw_needed=1;}
        else if((pressed&1)&&mx>=x+16&&mx<x+w-16&&my>=y+139&&my<y+144+count*34){int row=(my-(y+139))/34;if(row>=0&&row<count){ctx->search_selected=row;gui_search_execute();}redraw_needed=1;}
    } else if(ctx->view==GUI_VIEW_FILES){
        char names[24][VFS_NAME_LEN];unsigned char dirs[24];int count=vfs_gui_list(names,dirs,24);int w=(int)ctx->fb->width*3/4,h=(int)ctx->fb->height*3/4;if(w<520)w=(int)ctx->fb->width-24;if(h<340)h=(int)ctx->fb->height-24;int x=((int)ctx->fb->width-w)/2,y=((int)ctx->fb->height-h)/2;
        if(moved&&mx>=x+14&&mx<x+w-14){int item=-1;if(my>=y+76&&my<y+100)item=0;else if(my>=y+104&&my<y+104+count*28)item=1+(my-(y+104))/28;if(item>=0&&item<=count&&item!=ctx->files_item){ctx->files_item=item;redraw_needed=1;}}
        if((pressed&1)&&mx>=x+w-60&&my>=y&&my<y+36){ctx->view=GUI_VIEW_DESKTOP;redraw_needed=1;}
        else if((pressed&1)&&mx>=x+14&&mx<x+w-14&&my>=y+76&&my<y+104+count*28){gui_files_open_selected();redraw_needed=1;}
    } else if(ctx->view==GUI_VIEW_SETTINGS){
        int w=(int)ctx->fb->width*2/3,h=(int)ctx->fb->height*2/3;if(w<520)w=(int)ctx->fb->width-24;if(h<330)h=(int)ctx->fb->height-24;int x=((int)ctx->fb->width-w)/2,y=((int)ctx->fb->height-h)/2;
        if(moved&&mx>=x+14&&mx<x+w-14&&my>=y+76&&my<y+82+5*42){int item=(my-(y+76))/42;if(item<0)item=0;if(item>4)item=4;if(item!=ctx->settings_item){ctx->settings_item=item;redraw_needed=1;}}
        if((pressed&1)&&mx>=x+w-60&&my>=y&&my<y+36){ctx->view=GUI_VIEW_DESKTOP;redraw_needed=1;}
        else if((pressed&1)&&mx>=x+14&&mx<x+w-14&&my>=y+76&&my<y+82+5*42){gui_settings_select();redraw_needed=1;}
    } else if(ctx->view==GUI_VIEW_TERMINAL){
        int w=(int)ctx->fb->width*3/4,h=(int)ctx->fb->height*3/4;if(w<500)w=(int)ctx->fb->width-24;if(h<300)h=(int)ctx->fb->height-24;int x=((int)ctx->fb->width-w)/2,y=((int)ctx->fb->height-h)/2;
        if((pressed&1)&&mx>=x+w-60&&my>=y&&my<y+32){ctx->view=GUI_VIEW_DESKTOP;redraw_needed=1;}
    } else {
        int panel_h=ctx->fb->height>=720?52:44,panel_y=(int)ctx->fb->height-panel_h;
        if(moved&&my>=panel_y){int old=ctx->selected_app;if(mx>=8&&mx<=46){ctx->selected_app=0;ctx->message="Nexus";}else if(mx>=52&&mx<=144){ctx->message="Search";}else{int icon_size=30,gap=6,total=GUI_APP_COUNT*icon_size+(GUI_APP_COUNT-1)*gap,start=((int)ctx->fb->width-total)/2;for(int i=0;i<GUI_APP_COUNT;i++)if(mx>=start+i*(icon_size+gap)&&mx<start+i*(icon_size+gap)+icon_size){ctx->selected_app=i;break;}}if(old!=ctx->selected_app)redraw_needed=1;}
        if((pressed&1)&&my>=panel_y&&mx>=8&&mx<=46){ctx->selected_app=0;ctx->message="NexusOS: Files  Terminal  Settings";redraw_needed=1;}
        else if((pressed&1)&&my>=panel_y&&mx>=52&&mx<=144){gui_search_open();redraw_needed=1;}
        else if((pressed&1)&&my>=panel_y){int icon_size=30,gap=6,total=GUI_APP_COUNT*icon_size+(GUI_APP_COUNT-1)*gap,start=((int)ctx->fb->width-total)/2;for(int i=0;i<GUI_APP_COUNT;i++)if(mx>=start+i*(icon_size+gap)&&mx<start+i*(icon_size+gap)+icon_size){ctx->selected_app=i;gui_input_handle_key(GUI_KEY_ENTER);return;}}
    }
    if(now!=ctx->last_clock_second){ctx->last_clock_second=now;redraw_needed=1;}
    if(redraw_needed)redraw();
}
