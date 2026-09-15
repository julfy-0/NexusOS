#include "window.h"

static gui_window_t windows[GUI_WINDOW_MAX];
static int screen_w, screen_h;
static int active_id;
static int drag_id;
static int drag_dx, drag_dy;

static gui_window_t *slot(gui_window_id_t id) {
    if (id <= GUI_WINDOW_NONE || id >= GUI_WINDOW_MAX) return 0;
    return &windows[id];
}

void gui_window_manager_init(int width, int height) {
    screen_w = width;
    screen_h = height;
    active_id = GUI_WINDOW_NONE;
    drag_id = GUI_WINDOW_NONE;
    for (int i = 0; i < GUI_WINDOW_MAX; ++i) windows[i].id = GUI_WINDOW_NONE;
}

gui_window_t *gui_window_open(gui_window_id_t id, const char *title, int width, int height) {
    gui_window_t *w = slot(id);
    if (!w) return 0;
    if (width < 220) width = 220;
    if (height < 120) height = 120;
    if (width > screen_w - 24) width = screen_w - 24;
    if (height > screen_h - 24) height = screen_h - 24;
    if (!w->visible || w->width != width || w->height != height) {
        w->x = (screen_w - width) / 2;
        w->y = (screen_h - height) / 2;
        w->width = width;
        w->height = height;
    }
    w->id = id;
    w->title = title;
    w->visible = 1;
    gui_window_focus(id);
    return w;
}

void gui_window_close(gui_window_id_t id) {
    gui_window_t *w = slot(id);
    if (!w) return;
    w->visible = 0;
    w->focused = 0;
    if (active_id == (int)id) active_id = GUI_WINDOW_NONE;
    if (drag_id == (int)id) drag_id = GUI_WINDOW_NONE;
}

void gui_window_focus(gui_window_id_t id) {
    active_id = id;
    for (int i = 0; i < GUI_WINDOW_MAX; ++i) windows[i].focused = (i == (int)id && windows[i].visible);
}

gui_window_t *gui_window_get(gui_window_id_t id) {
    gui_window_t *w = slot(id);
    return (w && w->visible) ? w : 0;
}

int gui_window_active_id(void) { return active_id; }

static int inside(const gui_window_t *w, int x, int y) {
    return w && w->visible && x >= w->x && y >= w->y && x < w->x + w->width && y < w->y + w->height;
}

int gui_window_pointer(int mx, int my, uint8_t buttons, uint8_t pressed) {
    if (!(buttons & 1)) drag_id = GUI_WINDOW_NONE;
    if ((pressed & 1) && active_id != GUI_WINDOW_NONE) {
        gui_window_t *w = slot((gui_window_id_t)active_id);
        if (inside(w, mx, my)) {
            gui_window_focus(w->id);
            if (mx >= w->x + w->width - 34 && my < w->y + 32) return -active_id;
            if (my < w->y + 32) {
                drag_id = active_id;
                drag_dx = mx - w->x;
                drag_dy = my - w->y;
            }
        }
    }
    if (drag_id != GUI_WINDOW_NONE && (buttons & 1)) {
        gui_window_t *w = slot((gui_window_id_t)drag_id);
        if (w) {
            int nx = mx - drag_dx, ny = my - drag_dy;
            if (nx < 0) nx = 0;
            if (ny < 0) ny = 0;
            if (nx + w->width > screen_w) nx = screen_w - w->width;
            if (ny + w->height > screen_h) ny = screen_h - w->height;
            if (nx != w->x || ny != w->y) { w->x = nx; w->y = ny; return active_id; }
        }
    }
    return 0;
}
