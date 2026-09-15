#ifndef NEXUSOS_GUI_WINDOW_H
#define NEXUSOS_GUI_WINDOW_H

#include <stdint.h>

#define GUI_WINDOW_MAX 8

typedef enum {
    GUI_WINDOW_NONE = 0,
    GUI_WINDOW_TERMINAL = 1,
    GUI_WINDOW_SETTINGS = 2,
    GUI_WINDOW_FILES = 3,
    GUI_WINDOW_SEARCH = 4
} gui_window_id_t;

typedef struct {
    gui_window_id_t id;
    int x, y, width, height;
    int visible;
    int focused;
    const char *title;
} gui_window_t;

void gui_window_manager_init(int screen_width, int screen_height);
gui_window_t *gui_window_open(gui_window_id_t id, const char *title, int width, int height);
void gui_window_close(gui_window_id_t id);
void gui_window_focus(gui_window_id_t id);
gui_window_t *gui_window_get(gui_window_id_t id);
int gui_window_pointer(int mx, int my, uint8_t buttons, uint8_t pressed);
int gui_window_active_id(void);

#endif
