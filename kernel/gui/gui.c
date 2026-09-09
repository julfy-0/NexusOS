/* NexusOS graphical shell — framebuffer desktop, no window manager dependency. */
#include <stdint.h>
#include "gui.h"
#include "font8x16.h"
#include "mouse.h"
#include "nexus_version.h"

static nexus_framebuffer_t *fb;
static int g_gui_active = 0;
static int g_selected_app = 0;
static const char *g_gui_message = "Use Left/Right (or A/D), Enter to select, Esc to return";

/* Card layout — computed once in gui_draw_desktop, reused in gui_update. */
static int g_card_cx, g_card_cy, g_card_cw, g_card_ch, g_card_gap;

/* Returns app index (0-2) if (mx,my) is over a card, else -1. */
static int gui_hit_card(int32_t mx, int32_t my) {
    for (int i = 0; i < 3; i++) {
        int x = g_card_cx + i * (g_card_cw + g_card_gap);
        if (mx >= x && mx < x + g_card_cw &&
            my >= g_card_cy && my < g_card_cy + g_card_ch)
            return i;
    }
    return -1;
}

static uint8_t g_prev_buttons = 0;
static uint32_t pack(uint32_t rgb) {
    uint8_t r=(rgb>>16)&255,g=(rgb>>8)&255,b=rgb&255;
    if (fb->pixel_format == NEXUS_PIXFMT_BGR) return b | ((uint32_t)g<<8) | ((uint32_t)r<<16);
    return r | ((uint32_t)g<<8) | ((uint32_t)b<<16);
}
static void px(int x,int y,uint32_t c){ if(x<0||y<0||(uint32_t)x>=fb->width||(uint32_t)y>=fb->height)return; uint32_t *p=(uint32_t*)(uintptr_t)(fb->base+(uint64_t)y*fb->pixels_per_scanline*4);p[x]=pack(c); }
static void rect(int x,int y,int w,int h,uint32_t c){ for(int j=0;j<h;j++)for(int i=0;i<w;i++)px(x+i,y+j,c); }
static void glyph(int x,int y,char c,int scale,uint32_t top,uint32_t bottom){
    if(c<FONT_FIRST_CHAR||c>FONT_LAST_CHAR) return;
    const uint8_t *g=font8x16[(uint8_t)c-FONT_FIRST_CHAR];
    for(int gy=0;gy<16;gy++){ uint32_t t=(uint32_t)gy*255/15; uint8_t r=((top>>16)&255)*(255-t)/255+((bottom>>16)&255)*t/255; uint8_t gg=((top>>8)&255)*(255-t)/255+((bottom>>8)&255)*t/255; uint8_t b=(top&255)*(255-t)/255+(bottom&255)*t/255; uint32_t col=((uint32_t)r<<16)|((uint32_t)gg<<8)|b; for(int gx=0;gx<8;gx++) if((g[gy]>>(7-gx))&1) rect(x+gx*scale,y+gy*scale,scale,scale,col); }
}
static void text_center(const char *s,int y,int scale,uint32_t top,uint32_t bottom){ int n=0;while(s[n])n++; int w=n*8*scale; int x=((int)fb->width-w)/2; for(int i=0;i<n;i++)glyph(x+i*8*scale,y,s[i],scale,top,bottom); }
static void text_at(const char *s,int x,int y,int scale,uint32_t top,uint32_t bottom){ for(int i=0;s[i];i++)glyph(x+i*8*scale,y,s[i],scale,top,bottom); }
static void rounded_bar(int x,int y,int w,int h,int fill){ rect(x,y,w,h,0x252525); rect(x,y,fill,h,0xD0D0D0); }
static void draw_cursor(int x, int y) {
    static const char *shape[] = {
        "*", "**", "* *", "*  *", "*   *", "*    *",
        "*     *", "*      *", "*       *", "**********"
    };
    for (int row = 0; row < 10; ++row) {
        for (int col = 0; shape[row][col]; ++col) {
            if (shape[row][col] == '*') {
                px(x + col, y + row, 0xFFFFFF);
                if (x + col + 1 < (int)fb->width && y + row + 1 < (int)fb->height)
                    px(x + col + 1, y + row, 0x707070);
            }
        }
    }
}
void gui_init(nexus_framebuffer_t *f) {
    fb = f;
    if (f) mouse_set_screen_size(f->width, f->height);
    g_gui_active = 0;
    g_selected_app = 0;
    g_gui_message = "Use Left/Right (or A/D), Enter to select, Esc to return";
}

void gui_start(void) {
    if (!fb) return;
    g_gui_active = 1;
    g_selected_app = 0;
    g_prev_buttons = mouse_get_buttons(); /* don't treat held buttons as a click */
    g_gui_message = "Use Left/Right (or A/D), Enter to select, Esc to return";
    gui_draw_desktop();
}

void gui_exit(void) {
    g_gui_active = 0;
}

int gui_is_active(void) {
    return g_gui_active;
}

int gui_handle_key(int key) {
    if (!g_gui_active) return 0;

    if (key == GUI_KEY_ESCAPE) {
        gui_exit();
        return 1;
    }

    if (key == GUI_KEY_LEFT || key == 'a' || key == 'A') {
        g_selected_app = (g_selected_app + 2) % 3;
        g_gui_message = "Selected previous application";
        gui_draw_desktop();
        return 1;
    }
    if (key == GUI_KEY_RIGHT || key == 'd' || key == 'D') {
        g_selected_app = (g_selected_app + 1) % 3;
        g_gui_message = "Selected next application";
        gui_draw_desktop();
        return 1;
    }
    if (key == GUI_KEY_UP || key == 'w' || key == 'W' ||
        key == GUI_KEY_DOWN || key == 's' || key == 'S') {
        g_gui_message = "Keyboard navigation: use Left/Right to choose an app";
        gui_draw_desktop();
        return 1;
    }
    if (key == GUI_KEY_ENTER) {
        if (g_selected_app == 1) {
            gui_exit();
        } else if (g_selected_app == 0) {
            g_gui_message = "Files: application is not implemented yet";
            gui_draw_desktop();
        } else {
            g_gui_message = "Settings: application is not implemented yet";
            gui_draw_desktop();
        }
        return 1;
    }

    return 1;
}

void gui_status(const char *text){
    g_gui_message = text ? text : "";
    if (g_gui_active) gui_draw_desktop();
}

void gui_draw_desktop(void){
    if(!fb)return;
    rect(0,0,fb->width,fb->height,0x000000);
    rect(0,0,fb->width,42,0x090909);
    rect(0,41,fb->width,1,0x242424);
    text_center("NEXUS OS",(int)fb->height/2-150,4,0xFFFFFF,0x7A7A7A);
    text_center(NEXUS_VERSION_DISPLAY,(int)fb->height/2-92,1,0xCFCFCF,0x777777);
    text_center("GRAPHICAL SHELL",(int)fb->height/2-55,1,0xCFCFCF,0x777777);

    int w=(int)fb->width/3; if(w<260)w=260;if(w>560)w=560;
    int x=((int)fb->width-w)/2;
    int y=(int)fb->height/2+5;
    rounded_bar(x,y,w,8,w/3);

    const char *apps[3] = {"FILES", "TERMINAL", "SETTINGS"};
    int cy=y+58,cw=150,ch=78,gap=18,total=3*cw+2*gap,cx=((int)fb->width-total)/2;
    g_card_cx=cx; g_card_cy=cy; g_card_cw=cw; g_card_ch=ch; g_card_gap=gap;
    for(int i=0;i<3;i++){
        int card_x=cx+i*(cw+gap);
        uint32_t bg = (i==g_selected_app) ? 0x242424 : 0x111111;
        uint32_t border = (i==g_selected_app) ? 0xFFFFFF : 0x3A3A3A;
        rect(card_x,cy,cw,ch,bg);
        rect(card_x,cy,cw,2,border);
        rect(card_x,cy+ch-2,cw,2,border);
        int tw=5*8;
        text_at(apps[i],card_x+(cw-tw)/2,cy+30,1,
                i==g_selected_app ? 0xFFFFFF : 0xEEEEEE,
                i==g_selected_app ? 0xB0B0B0 : 0x9A9A9A);
    }

    text_center(g_gui_message,cy+105,1,0xBEBEBE,0x707070);

    int dw=360,dh=48,dx=((int)fb->width-dw)/2,dy=(int)fb->height-dh-26;
    rect(dx,dy,dw,dh,0x101010);
    rect(dx,dy,dw,1,0x353535);
    text_center("NEXUS",dy+16,1,0xFFFFFF,0x888888);
    if (mouse_is_present()) draw_cursor(mouse_get_x(), mouse_get_y());
}

void gui_update(void) {
    if (!fb) return;

    int redraw = mouse_has_moved();

    if (mouse_is_present()) {
        int32_t mx = mouse_get_x();
        int32_t my = mouse_get_y();
        uint8_t buttons = mouse_get_buttons();
        uint8_t pressed = buttons & ~g_prev_buttons; /* newly-pressed this tick */

        /* Hover: highlight the card under the cursor. */
        int hovered = gui_hit_card(mx, my);
        if (hovered >= 0 && hovered != g_selected_app) {
            g_selected_app = hovered;
            g_gui_message = "Click to open";
            redraw = 1;
        }

        /* Left-click (bit 0). */
        if (pressed & 0x01) {
            int clicked = gui_hit_card(mx, my);
            if (clicked >= 0) {
                if (clicked == 1) {          /* TERMINAL */
                    gui_exit();
                } else if (clicked == 0) {   /* FILES */
                    g_gui_message = "Files: not implemented yet";
                    redraw = 1;
                } else {                      /* SETTINGS */
                    g_gui_message = "Settings: not implemented yet";
                    redraw = 1;
                }
            }
        }

        g_prev_buttons = buttons;
    }

    if (redraw) {
        gui_draw_desktop();
        mouse_clear_moved();
    }
}
