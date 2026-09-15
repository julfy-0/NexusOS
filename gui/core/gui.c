/* NexusOS graphical desktop — version comes from nexus_version.h. */
#include <stdint.h>
#include "gui.h"
#include "font.h"
#include "mouse.h"
#include "pit.h"
#include "nexus_version.h"
#include "vfs.h"
#include "window.h"

extern const uint8_t _binary_assets_wallpapers_nexus_default_rgb565_start[];
extern const uint8_t _binary_assets_wallpapers_nexus_default_rgb565_end[];

#define WALLPAPER_W 480
#define WALLPAPER_H 270
#define APP_COUNT 10

static nexus_framebuffer_t *fb;
static int g_gui_active = 0;
static int g_selected_app = 0;
static int g_view = 0; /* 0 = desktop, 1 = terminal, 2 = settings, 3 = files, 4 = search */
static char g_search_query[64];
static int g_search_len = 0;
static int g_search_selected = 0;
static int g_files_item = 0;
static uint8_t g_prev_buttons = 0;
static int g_settings_item = 0;
static char g_term_line[96];
static int g_term_len = 0;
static const char *g_term_output = "Welcome to NexusOS Terminal";
static uint64_t g_last_clock_second = (uint64_t)-1;
static const char *g_gui_message = "Left/Right: select  Enter: open  Esc: return";

static uint32_t pack(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
    if (fb->pixel_format == NEXUS_PIXFMT_BGR)
        return b | ((uint32_t)g << 8) | ((uint32_t)r << 16);
    return r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

static uint32_t unpack(uint32_t pixel) {
    uint8_t a = pixel & 255, b = (pixel >> 8) & 255, c = (pixel >> 16) & 255;
    if (fb->pixel_format == NEXUS_PIXFMT_BGR)
        return ((uint32_t)a << 16) | ((uint32_t)b << 8) | c;
    return ((uint32_t)c << 16) | ((uint32_t)b << 8) | a;
}

static void px(int x, int y, uint32_t c) {
    if (!fb || x < 0 || y < 0 || (uint32_t)x >= fb->width || (uint32_t)y >= fb->height) return;
    uint32_t *p = (uint32_t *)(uintptr_t)(fb->base + (uint64_t)y * fb->pixels_per_scanline * 4);
    p[x] = pack(c);
}

static void blend_px(int x, int y, uint32_t c, uint8_t alpha) {
    if (!fb || x < 0 || y < 0 || (uint32_t)x >= fb->width || (uint32_t)y >= fb->height) return;
    uint32_t *p = (uint32_t *)(uintptr_t)(fb->base + (uint64_t)y * fb->pixels_per_scanline * 4);
    uint32_t old = unpack(p[x]);
    uint32_t r = (((c >> 16) & 255) * alpha + ((old >> 16) & 255) * (255 - alpha)) / 255;
    uint32_t g = (((c >> 8) & 255) * alpha + ((old >> 8) & 255) * (255 - alpha)) / 255;
    uint32_t b = ((c & 255) * alpha + (old & 255) * (255 - alpha)) / 255;
    p[x] = pack((r << 16) | (g << 8) | b);
}

static void rect(int x, int y, int w, int h, uint32_t c) {
    for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) px(x + i, y + j, c);
}

static void rect_alpha(int x, int y, int w, int h, uint32_t c, uint8_t alpha) {
    for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) blend_px(x + i, y + j, c, alpha);
}

static void rounded_rect_alpha(int x, int y, int w, int h, int r, uint32_t c, uint8_t alpha) {
    if (r < 1) { rect_alpha(x, y, w, h, c, alpha); return; }
    rect_alpha(x + r, y, w - 2 * r, h, c, alpha);
    rect_alpha(x, y + r, r, h - 2 * r, c, alpha);
    rect_alpha(x + w - r, y + r, r, h - 2 * r, c, alpha);
    for (int yy = 0; yy < r; ++yy) for (int xx = 0; xx < r; ++xx) {
        int dx = xx - r + 1, dy = yy - r + 1;
        if (dx * dx + dy * dy <= r * r) {
            blend_px(x + xx, y + yy, c, alpha);
            blend_px(x + w - 1 - xx, y + yy, c, alpha);
            blend_px(x + xx, y + h - 1 - yy, c, alpha);
            blend_px(x + w - 1 - xx, y + h - 1 - yy, c, alpha);
        }
    }
}

static void circle(int cx, int cy, int r, uint32_t c, uint8_t alpha) {
    int rr = r * r;
    for (int y = -r; y <= r; ++y) for (int x = -r; x <= r; ++x)
        if (x * x + y * y <= rr) blend_px(cx + x, cy + y, c, alpha);
}

static void glyph(int x, int y, char c, int scale, uint32_t color) {
    const uint8_t *g = gui_font_glyph(c);
    if (!g) return;
    for (int gy = 0; gy < 16; ++gy)
        for (int gx = 0; gx < 8; ++gx)
            if ((g[gy] >> (7 - gx)) & 1)
                rect(x + gx * scale, y + gy * scale, scale, scale, color);
}

static void text_at(const char *s, int x, int y, int scale, uint32_t color) {
    for (int i = 0; s[i]; ++i) glyph(x + i * 8 * scale, y, s[i], scale, color);
}

static int text_width(const char *s, int scale) {
    int n = 0; while (s[n]) ++n; return n * 8 * scale;
}

static void text_center(const char *s, int y, int scale, uint32_t color) {
    text_at(s, ((int)fb->width - text_width(s, scale)) / 2, y, scale, color);
}

static void draw_wallpaper(void) {
    const uint8_t *src = _binary_assets_wallpapers_nexus_default_rgb565_start;
    for (uint32_t y = 0; y < fb->height; ++y) {
        uint32_t sy = (uint64_t)y * WALLPAPER_H / fb->height;
        for (uint32_t x = 0; x < fb->width; ++x) {
            uint32_t sx = (uint64_t)x * WALLPAPER_W / fb->width;
            uint32_t off = (sy * WALLPAPER_W + sx) * 2;
            uint16_t v = (uint16_t)src[off] | ((uint16_t)src[off + 1] << 8);
            uint32_t r = ((v >> 11) & 31) * 255 / 31;
            uint32_t g = ((v >> 5) & 63) * 255 / 63;
            uint32_t b = (v & 31) * 255 / 31;
            px((int)x, (int)y, (r << 16) | (g << 8) | b);
        }
    }
}

static void draw_search_icon(int cx, int cy) {
    for (int y = -7; y <= 7; ++y) for (int x = -7; x <= 7; ++x) {
        int d = x * x + y * y;
        if (d >= 34 && d <= 58) blend_px(cx + x, cy + y, 0xE8E8EF, 230);
    }
    for (int i = 0; i < 7; ++i) blend_px(cx + 6 + i, cy + 6 + i, 0xE8E8EF, 230);
}

static void draw_nexus_button(int cx, int cy) {
    circle(cx, cy, 13, 0x17151F, 190);
    for (int y = -8; y <= 8; ++y) for (int x = -8; x <= 8; ++x) {
        int d = x*x + y*y;
        if (d >= 43 && d <= 64) blend_px(cx + x, cy + y, 0xF1EFF6, 230);
    }
}

static void draw_app_icon(int x, int y, int selected, int index) {
    uint32_t bg = selected ? 0xF0F0F3 : 0xD4D4D8;
    uint8_t alpha = selected ? 245 : 220;
    rect_alpha(x, y, 30, 30, bg, alpha);
    if (selected) {
        rect_alpha(x - 3, y - 3, 36, 2, 0xFFFFFF, 170);
        rect_alpha(x - 3, y + 31, 36, 2, 0xFFFFFF, 170);
    }
    /* Minimal visual variation while real app icons are not implemented yet. */
    if (index == 0) rect_alpha(x + 7, y + 8, 16, 12, 0x7A728D, 180);
    else if (index == 1) text_at(">_", x + 5, y + 7, 1, 0x6A6178);
    else if (index == 2) {
        circle(x + 15, y + 15, 7, 0x6A6178, 180);
        circle(x + 15, y + 15, 3, bg, 245);
    }
}

static const char *app_label(int index) {
    static const char *labels[APP_COUNT] = {
        "Files", "Terminal", "Settings", "App 4", "App 5",
        "App 6", "App 7", "App 8", "App 9", "App 10"
    };
    return (index >= 0 && index < APP_COUNT) ? labels[index] : "";
}

static void draw_uptime(int x, int y) {
    uint64_t s = pit_get_uptime_seconds();
    uint64_t h = (s / 3600) % 24, m = (s / 60) % 60;
    char buf[6] = {'0' + (char)(h / 10), '0' + (char)(h % 10), ':', '0' + (char)(m / 10), '0' + (char)(m % 10), 0};
    text_at(buf, x, y, 2, 0xF3F2F7);
    text_at("NEXUSOS", x + 2, y + 21, 1, 0xD5D0DD);
}


static int streq(const char *a, const char *b) { int i=0; while(a[i]&&b[i]&&a[i]==b[i]) i++; return a[i]==0 && b[i]==0; }


static const char *g_search_names[] = {"Files", "Terminal", "Settings", "Reboot", "Shutdown"};
#define SEARCH_COUNT 5

static int search_match(const char *name) {
    if (!g_search_len) return 1;
    for (int i = 0; name[i]; ++i) {
        int j = 0;
        while (g_search_query[j] && name[i+j]) {
            char a = name[i+j], b = g_search_query[j];
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) break;
            ++j;
        }
        if (!g_search_query[j]) return 1;
    }
    return 0;
}

static int search_visible_count(void) {
    int n=0; for(int i=0;i<SEARCH_COUNT;i++) if(search_match(g_search_names[i])) n++;
    return n;
}

static int search_result_at(int selected) {
    int n=0;
    for(int i=0;i<SEARCH_COUNT;i++) if(search_match(g_search_names[i])) {
        if(n++==selected) return i;
    }
    return -1;
}

static gui_window_t *window_for(int id, const char *title, int w, int h) {
    return gui_window_open((gui_window_id_t)id, title, w, h);
}

static void draw_window_frame(gui_window_t *win) {
    rect_alpha(win->x + 2, win->y + 3, win->width, win->height, 0x000000, 70);
    rect_alpha(win->x, win->y, win->width, win->height, 0x17141F, 238);
    rect_alpha(win->x, win->y, win->width, 32, 0x302A3B, 248);
    rect_alpha(win->x + win->width - 30, win->y + 8, 16, 16, 0x7A4055, 220);
    text_at("X", win->x + win->width - 27, win->y + 8, 1, 0xFFFFFF);
    if (win->title) text_at(win->title, win->x + 14, win->y + 8, 1, 0xF4F1F8);
}

static void draw_search(void) {
    draw_wallpaper();
    int w=(int)fb->width*2/3, h=(int)fb->height*2/3;
    if(w<500) w=(int)fb->width-24;
    if(h<280) h=(int)fb->height-24;
    gui_window_t *win=window_for(GUI_WINDOW_SEARCH,"NexusOS Search",w,h); if(!win)return; int x=win->x,y=win->y; w=win->width; h=win->height;
    draw_window_frame(win);
    rect_alpha(x+18,y+58,w-36,34,0x383240,235);
    draw_search_icon(x+36,y+75);
    text_at("> ",x+54,y+68,1,0xFFFFFF);
    text_at(g_search_query,x+70,y+68,1,0xFFFFFF);
    text_at("_",x+70+g_search_len*8,y+68,1,0xBFAEFF);
    text_at("Applications & System",x+20,y+116,1,0xBFAEFF);
    int row=0;
    for(int i=0;i<SEARCH_COUNT;i++) if(search_match(g_search_names[i])) {
        int iy=y+144+row*34;
        if(row==g_search_selected) rect_alpha(x+16,iy-5,w-32,28,0x5A506B,180);
        text_at(g_search_names[i],x+32,iy,1,row==g_search_selected?0xFFFFFF:0xD8D3E0);
        row++;
    }
    if(!row) text_at("No results",x+32,y+148,1,0xAFA8BC);
    text_at("Type to search  Up/Down: select  Enter: open  Esc: desktop",x+20,y+h-28,1,0xAFA8BC);
}

static void search_execute(void) {
    int r=search_result_at(g_search_selected);
    if(r==0) { g_view=3; g_files_item=0; }
    else if(r==1) { g_view=1; g_term_len=0; g_term_line[0]=0; g_term_output="Welcome to NexusOS Terminal"; }
    else if(r==2) { g_view=2; g_settings_item=0; }
    else if(r==3) { g_gui_message="Reboot is available from NexusOS Command Line"; g_view=0; }
    else if(r==4) { g_gui_message="Shutdown is available from NexusOS Command Line"; g_view=0; }
}

static void draw_terminal(void) {
    draw_wallpaper();
    int w=(int)fb->width*3/4, h=(int)fb->height*3/4;
    if (w < 500) w = (int)fb->width - 24;
    if (h < 300) h = (int)fb->height - 24;
    gui_window_t *win=window_for(GUI_WINDOW_TERMINAL,"NexusOS Terminal",w,h); if(!win)return; int x=win->x,y=win->y; w=win->width; h=win->height;
    draw_window_frame(win);
    text_at("NexusOS " NEXUS_VERSION_STRING " Terminal",x+16,y+52,1,0xCFC8DC);
    text_at(g_term_output,x+16,y+78,1,0xEEEAF4);
    text_at("Commands: help  clear  version  uptime  desktop",x+16,y+h-64,1,0xAFA8BC);
    text_at("NexusOS> ",x+16,y+h-36,1,0xFFFFFF);
    text_at(g_term_line,x+16+9*8,y+h-36,1,0xFFFFFF);
    text_at("_",x+16+(9+g_term_len)*8,y+h-36,1,0xBFAEFF);
}


static void draw_settings(void) {
    draw_wallpaper();
    int w=(int)fb->width*2/3, h=(int)fb->height*2/3;
    if (w < 520) w=(int)fb->width-24;
    if (h < 330) h=(int)fb->height-24;
    gui_window_t *win=window_for(GUI_WINDOW_SETTINGS,"NexusOS Settings",w,h); if(!win)return; int x=win->x,y=win->y; w=win->width; h=win->height;
    draw_window_frame(win);
    text_at("SYSTEM",x+20,y+58,1,0xBFAEFF);
    const char *items[5] = {"Version   NexusOS " NEXUS_VERSION_DISPLAY "","Display   Automatic framebuffer resolution","Input     PS/2 Keyboard and Mouse","Uptime    System runtime","About     NexusOS x86_64 / UEFI"};
    for(int i=0;i<5;i++) {
        int iy=y+82+i*42;
        if(i==g_settings_item) rect_alpha(x+14,iy-6,w-28,32,0x5A506B,170);
        text_at(items[i],x+28,iy,1,i==g_settings_item?0xFFFFFF:0xD8D3E0);
    }
    text_at("Up/Down: select   Enter: info   Esc: desktop",x+20,y+h-28,1,0xAFA8BC);
}


static void draw_files(void) {
    char names[24][VFS_NAME_LEN]; unsigned char dirs[24]; char cwd[128];
    int count = vfs_gui_list(names, dirs, 24); vfs_gui_getcwd(cwd, sizeof(cwd));
    draw_wallpaper();
    int w=(int)fb->width*3/4, h=(int)fb->height*3/4;
    if(w<520) w=(int)fb->width-24;
    if(h<340) h=(int)fb->height-24;
    gui_window_t *win=window_for(GUI_WINDOW_FILES,"NexusOS Files",w,h); if(!win)return; int x=win->x,y=win->y; w=win->width; h=win->height;
    draw_window_frame(win);
    text_at(cwd,x+18,y+52,1,0xBFAEFF);
    text_at("..",x+28,y+82,1,0xD8D3E0);
    if(g_files_item==0) rect_alpha(x+14,y+76,w-28,24,0x5A506B,170);
    for(int i=0;i<count && i<12;i++) {
        int iy=y+108+i*28, sel=i+1==g_files_item;
        if(sel) rect_alpha(x+14,iy-4,w-28,24,0x5A506B,170);
        text_at(dirs[i]?"[DIR]":"[FILE]",x+28,iy,1,dirs[i]?0xC9B9FF:0xBFC3D0);
        text_at(names[i],x+90,iy,1,sel?0xFFFFFF:0xD8D3E0);
    }
    if(!count) text_at("(empty)",x+28,y+112,1,0xAFA8BC);
    text_at("Up/Down: select  Enter: open  Backspace: up  Esc: desktop",x+18,y+h-28,1,0xAFA8BC);
}

static void draw_cursor(void) {
    int mx=mouse_get_x(), my=mouse_get_y();
    for(int i=0;i<12;i++) for(int j=0;j<=i/2;j++) px(mx+j,my+i,0xFFFFFF);
    for(int i=0;i<12;i++) px(mx,my+i,0x1A1622);
}

static void files_open_selected(void) {
    char names[24][VFS_NAME_LEN]; unsigned char dirs[24];
    int count=vfs_gui_list(names,dirs,24);
    if(g_files_item==0) { vfs_cd(".."); g_files_item=0; }
    else { int i=g_files_item-1; if(i<count && dirs[i]) { vfs_cd(names[i]); g_files_item=0; } }
}

static void terminal_execute(void) {
    g_term_line[g_term_len]=0;
    if (streq(g_term_line,"help")) g_term_output="help: clear version uptime desktop";
    else if (streq(g_term_line,"clear")) g_term_output="";
    else if (streq(g_term_line,"version")) g_term_output="NexusOS " NEXUS_VERSION_DISPLAY "";
    else if (streq(g_term_line,"uptime")) g_term_output="System uptime available in Desktop panel";
    else if (streq(g_term_line,"desktop")) { g_view=0; g_term_len=0; g_term_line[0]=0; gui_draw_desktop(); return; }
    else if (g_term_len) g_term_output="Unknown command. Type help";
    g_term_len=0; g_term_line[0]=0;
}

void gui_init(nexus_framebuffer_t *f) {
    fb = f;
    if (f) { mouse_set_screen_size(f->width, f->height); gui_window_manager_init((int)f->width, (int)f->height); }
    g_gui_active = 0;
    g_selected_app = 0;
}

void gui_start(void) {
    if (!fb) return;
    g_gui_active = 1;
    g_selected_app = 0;
    g_last_clock_second = (uint64_t)-1;
    g_prev_buttons = mouse_get_buttons();
    g_search_len=0; g_search_query[0]=0; g_search_selected=0;
    gui_draw_desktop();
}

void gui_exit(void) { g_gui_active = 0; g_view = 0; }
int gui_is_active(void) { return g_gui_active; }

int gui_handle_key(int key) {
    if (!g_gui_active) return 0;
    if (g_view == 4) {
        int count=search_visible_count();
        if(key==GUI_KEY_ESCAPE){g_view=0;gui_draw_desktop();return 1;}
        if(key==GUI_KEY_UP||key=='w'||key=='W'){if(count)g_search_selected=(g_search_selected+count-1)%count;gui_draw_desktop();return 1;}
        if(key==GUI_KEY_DOWN||key=='s'||key=='S'){if(count)g_search_selected=(g_search_selected+1)%count;gui_draw_desktop();return 1;}
        if(key==GUI_KEY_ENTER){if(count)search_execute();gui_draw_desktop();return 1;}
        if(key=='\b'){if(g_search_len)g_search_query[--g_search_len]=0;g_search_selected=0;gui_draw_desktop();return 1;}
        if(key>=32&&key<127&&g_search_len<(int)sizeof(g_search_query)-1){g_search_query[g_search_len++]=(char)key;g_search_query[g_search_len]=0;g_search_selected=0;gui_draw_desktop();return 1;}
        return 1;
    }
    if (g_view == 3) {
        char names[24][VFS_NAME_LEN]; unsigned char dirs[24]; int count=vfs_gui_list(names,dirs,24); int total=count+1;
        if(key==GUI_KEY_ESCAPE){g_view=0;gui_draw_desktop();return 1;}
        if(key==GUI_KEY_UP||key=='w'||key=='W'){g_files_item=(g_files_item+total-1)%total;gui_draw_desktop();return 1;}
        if(key==GUI_KEY_DOWN||key=='s'||key=='S'){g_files_item=(g_files_item+1)%total;gui_draw_desktop();return 1;}
        if(key==GUI_KEY_ENTER){files_open_selected();gui_draw_desktop();return 1;}
        if(key=='\b'){vfs_cd("..");g_files_item=0;gui_draw_desktop();return 1;} return 1;
    }
    if (g_view == 2) {
        if (key == GUI_KEY_ESCAPE) { g_view=0; gui_draw_desktop(); return 1; }
        if (key == GUI_KEY_UP || key == 'w' || key == 'W') { g_settings_item=(g_settings_item+4)%5; gui_draw_desktop(); return 1; }
        if (key == GUI_KEY_DOWN || key == 's' || key == 'S') { g_settings_item=(g_settings_item+1)%5; gui_draw_desktop(); return 1; }
        if (key == GUI_KEY_ENTER) { g_gui_message="Settings item selected - editing comes in a future update"; return 1; }
        return 1;
    }
    if (g_view == 1) {
        if (key == GUI_KEY_ESCAPE) { g_view=0; gui_draw_desktop(); return 1; }
        if (key == GUI_KEY_ENTER || key == '\n') { terminal_execute(); gui_draw_desktop(); return 1; }
        if (key == '\b') { if(g_term_len) g_term_line[--g_term_len]=0; gui_draw_desktop(); return 1; }
        if (key >= 32 && key < 127 && g_term_len < (int)sizeof(g_term_line)-1) { g_term_line[g_term_len++]=(char)key; g_term_line[g_term_len]=0; gui_draw_desktop(); }
        return 1;
    }
    if (key == GUI_KEY_ESCAPE) { gui_exit(); return 1; }
    if (key == '/' || key == 'q' || key == 'Q') { g_view=4; g_search_len=0; g_search_query[0]=0; g_search_selected=0; gui_draw_desktop(); return 1; }
    if (key == GUI_KEY_LEFT || key == 'a' || key == 'A') { g_selected_app=(g_selected_app+APP_COUNT-1)%APP_COUNT; gui_draw_desktop(); return 1; }
    if (key == GUI_KEY_RIGHT || key == 'd' || key == 'D') { g_selected_app=(g_selected_app+1)%APP_COUNT; gui_draw_desktop(); return 1; }
    if (key == GUI_KEY_UP || key == GUI_KEY_DOWN || key == 'w' || key == 'W' || key == 's' || key == 'S') return 1;
    if (key == GUI_KEY_ENTER) {
        if (g_selected_app == 1) { g_view=1; g_term_len=0; g_term_line[0]=0; g_term_output="Welcome to NexusOS Terminal"; gui_draw_desktop(); return 1; }
        if (g_selected_app == 0) { g_view=3; g_files_item=0; gui_draw_desktop(); return 1; }
        else if (g_selected_app == 2) { g_view=2; g_settings_item=0; gui_draw_desktop(); return 1; }
        else g_gui_message="Application slot reserved for NexusOS " NEXUS_VERSION_STRING;
        gui_draw_desktop(); return 1;
    }
    return 1;
}
void gui_status(const char *text) {
    g_gui_message = text ? text : "";
    if (g_gui_active) gui_draw_desktop();
}

void gui_draw_desktop(void) {
    if (!fb) return;
    if (g_view == 1) { draw_terminal(); return; }
    if (g_view == 4) { draw_search(); return; }
    if (g_view == 3) { draw_files(); return; }
    if (g_view == 2) { draw_settings(); return; }
    if (g_view == 3) { draw_files(); return; }
    draw_wallpaper();

    /* Floating glass panel: responsive and centered with generous margins. */
    int panel_h = fb->height >= 720 ? 58 : 48;
    int margin = fb->width >= 900 ? 22 : 10;
    int panel_y = (int)fb->height - panel_h - margin;
    int panel_w = (int)fb->width - margin * 2;
    rounded_rect_alpha(margin + 2, panel_y + 3, panel_w, panel_h, 14, 0x000000, 80);
    rounded_rect_alpha(margin, panel_y, panel_w, panel_h, 14, 0x17141F, 218);
    rect_alpha(margin + 14, panel_y, panel_w - 28, 1, 0xD6CCEB, 75);

    int cy = panel_y + panel_h / 2;
    draw_nexus_button(margin + 30, cy);

    rounded_rect_alpha(margin + 58, cy - 16, 96, 32, 10, 0x3B3547, 185);
    draw_search_icon(margin + 132, cy);
    text_at("Search", margin + 68, cy - 5, 1, 0xD9D4E2);

    int icon_size = 32, gap = 8;
    int total = APP_COUNT * icon_size + (APP_COUNT - 1) * gap;
    int start = ((int)fb->width - total) / 2;
    for (int i = 0; i < APP_COUNT; ++i)
        draw_app_icon(start + i * (icon_size + gap), cy - icon_size / 2, i == g_selected_app, i);

    /* Active application name above the dock. */
    if (g_selected_app >= 0 && g_selected_app < 3)
        text_center(app_label(g_selected_app), panel_y - 34, 1, 0xF3EFF8);

    int clock_w = 98;
    int clock_x = margin + panel_w - clock_w - 12;
    rounded_rect_alpha(clock_x - 8, cy - 20, clock_w + 8, 40, 10, 0x282331, 145);
    draw_uptime(clock_x, cy - 18);

    if (g_gui_message && g_gui_message[0])
        text_center(g_gui_message, panel_y - 18, 1, 0xD7D1E1);
    draw_cursor();
}

void gui_update(void) {
    if (!g_gui_active || !fb) return;

    uint64_t now = pit_get_uptime_seconds();
    int redraw = 0;
    int moved = mouse_has_moved();
    if (moved) { mouse_clear_moved(); redraw = 1; }

    int mx = mouse_get_x(), my = mouse_get_y();
    uint8_t buttons = mouse_get_buttons();
    uint8_t pressed = buttons & (uint8_t)~g_prev_buttons;
    g_prev_buttons = buttons;
    int window_event = gui_window_pointer(mx, my, buttons, pressed);
    if (window_event < 0) { gui_window_close((gui_window_id_t)(-window_event)); g_view = 0; redraw = 1; }
    else if (window_event > 0) redraw = 1;

    /* Mouse polish for Search: hover selects rows, click executes. */
    if (g_view == 4) {
        int w=(int)fb->width*2/3, h=(int)fb->height*2/3;
        if(w<500) w=(int)fb->width-24;
        if(h<280) h=(int)fb->height-24;
        int x=((int)fb->width-w)/2, y=((int)fb->height-h)/2;
        int count=search_visible_count();
        if (moved && mx>=x+16 && mx<x+w-16 && my>=y+139 && my<y+144+count*34) {
            int row=(my-(y+139))/34;
            if(row>=0 && row<count && row!=g_search_selected) { g_search_selected=row; redraw=1; }
        }
        if ((pressed&1) && mx>=x+w-60 && my>=y && my<y+42) { g_view=0; redraw=1; }
        else if ((pressed&1) && mx>=x+16 && mx<x+w-16 && my>=y+139 && my<y+144+count*34) {
            int row=(my-(y+139))/34; if(row>=0 && row<count){g_search_selected=row;search_execute();} redraw=1;
        }
    }
    /* Files: hover and click rows, including parent directory. */
    else if (g_view == 3) {
        char names[24][VFS_NAME_LEN]; unsigned char dirs[24]; int count=vfs_gui_list(names,dirs,24);
        int w=(int)fb->width*3/4, h=(int)fb->height*3/4;
        if(w<520) w=(int)fb->width-24;
        if(h<340) h=(int)fb->height-24;
        int x=((int)fb->width-w)/2, y=((int)fb->height-h)/2;
        if (moved && mx>=x+14 && mx<x+w-14) {
            int item=-1;
            if(my>=y+76 && my<y+100) item=0;
            else if(my>=y+104 && my<y+104+count*28) item=1+(my-(y+104))/28;
            if(item>=0 && item<=count && item!=g_files_item){g_files_item=item;redraw=1;}
        }
        if ((pressed&1) && mx>=x+w-60 && my>=y && my<y+36) { g_view=0; redraw=1; }
        else if ((pressed&1) && mx>=x+14 && mx<x+w-14 && my>=y+76 && my<y+104+count*28) { files_open_selected(); redraw=1; }
    }
    /* Settings: hover and click selectable rows; title ESC area closes. */
    else if (g_view == 2) {
        int w=(int)fb->width*2/3, h=(int)fb->height*2/3;
        if(w<520) w=(int)fb->width-24;
        if(h<330) h=(int)fb->height-24;
        int x=((int)fb->width-w)/2, y=((int)fb->height-h)/2;
        if(moved && mx>=x+14 && mx<x+w-14 && my>=y+76 && my<y+82+5*42){
            int item=(my-(y+76))/42; if(item<0)item=0; if(item>4)item=4;
            if(item!=g_settings_item){g_settings_item=item;redraw=1;}
        }
        if((pressed&1) && mx>=x+w-60 && my>=y && my<y+36){g_view=0;redraw=1;}
        else if((pressed&1) && mx>=x+14 && mx<x+w-14 && my>=y+76 && my<y+82+5*42){
            g_gui_message="Settings item selected"; redraw=1;
        }
    }
    /* Terminal: mouse can close via ESC title area and focus input area. */
    else if (g_view == 1) {
        int w=(int)fb->width*3/4, h=(int)fb->height*3/4;
        if(w<500) w=(int)fb->width-24;
        if(h<300) h=(int)fb->height-24;
        int x=((int)fb->width-w)/2, y=((int)fb->height-h)/2;
        if((pressed&1) && mx>=x+w-60 && my>=y && my<y+32){g_view=0;redraw=1;}
    }
    /* Desktop: hover highlights controls; edge-triggered click launches. */
    else {
        int panel_h=fb->height>=720?52:44, panel_y=(int)fb->height-panel_h;
        if(moved && my>=panel_y) {
            int old=g_selected_app;
            if(mx>=8 && mx<=46) { g_selected_app=0; g_gui_message="Nexus"; }
            else if(mx>=52 && mx<=144) { g_gui_message="Search"; }
            else {
                int icon_size=30,gap=6,total=APP_COUNT*icon_size+(APP_COUNT-1)*gap;
                int start=((int)fb->width-total)/2;
                for(int i=0;i<APP_COUNT;i++)
                    if(mx>=start+i*(icon_size+gap) && mx<start+i*(icon_size+gap)+icon_size) {
                        g_selected_app=i; break;
                    }
            }
            if(old!=g_selected_app) redraw=1;
        }
        if((pressed&1) && my>=panel_y && mx>=8 && mx<=46) {
            /* Nexus button currently acts as a safe desktop home selector. */
            g_selected_app=0; g_gui_message="NexusOS: Files  Terminal  Settings"; redraw=1;
        } else if((pressed&1) && my>=panel_y && mx>=52 && mx<=144) {
            g_view=4; g_search_len=0; g_search_query[0]=0; g_search_selected=0; redraw=1;
        } else if((pressed&1) && my>=panel_y) {
            int icon_size=30,gap=6,total=APP_COUNT*icon_size+(APP_COUNT-1)*gap;
            int start=((int)fb->width-total)/2;
            for(int i=0;i<APP_COUNT;i++)
                if(mx>=start+i*(icon_size+gap) && mx<start+i*(icon_size+gap)+icon_size) {
                    g_selected_app=i; gui_handle_key(GUI_KEY_ENTER); return;
                }
        }
    }

    if(now!=g_last_clock_second){g_last_clock_second=now;redraw=1;}
    if(redraw) gui_draw_desktop();
}

