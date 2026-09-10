#ifndef NEXUSOS_GUI_STATE_H
#define NEXUSOS_GUI_STATE_H

#include <stdint.h>
#include "boot_info.h"

#define GUI_APP_COUNT 10
#define GUI_SEARCH_COUNT 5

typedef enum {
    GUI_VIEW_DESKTOP = 0,
    GUI_VIEW_TERMINAL = 1,
    GUI_VIEW_SETTINGS = 2,
    GUI_VIEW_FILES = 3,
    GUI_VIEW_SEARCH = 4
} gui_view_t;

typedef struct {
    nexus_framebuffer_t *fb;
    int active;
    int selected_app;
    gui_view_t view;

    char search_query[64];
    int search_len;
    int search_selected;

    int files_item;
    int settings_item;

    char term_line[96];
    int term_len;
    const char *term_output;

    uint8_t prev_buttons;
    uint64_t last_clock_second;
    const char *message;
} nexus_gui_context_t;

nexus_gui_context_t *gui_context(void);

#endif
