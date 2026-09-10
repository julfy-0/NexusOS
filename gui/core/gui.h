#ifndef NEXUSOS_GUI_H
#define NEXUSOS_GUI_H

#include "boot_info.h"

/* Public GUI lifecycle and input API. Hardware-specific input remains in drivers. */
#define GUI_KEY_ESCAPE    27
#define GUI_KEY_ENTER     10
#define GUI_KEY_LEFT      0x81
#define GUI_KEY_RIGHT     0x82
#define GUI_KEY_UP        0x83
#define GUI_KEY_DOWN      0x84

void gui_init(nexus_framebuffer_t *fb);
void gui_start(void);
void gui_exit(void);
int gui_is_active(void);
int gui_handle_key(int key);
void gui_draw_desktop(void);
void gui_update(void);
void gui_status(const char *text);

#endif
