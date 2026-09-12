#ifndef NEXUSOS_KEYBOARD_H
#define NEXUSOS_KEYBOARD_H
#include <stdint.h>

void keyboard_init(void);
void keyboard_handle_irq(void);
void keyboard_process_scancode(uint8_t scancode);
int keyboard_is_present(void);

int keyboard_shift_down(void);
int keyboard_ctrl_down(void);
int keyboard_alt_down(void);
int keyboard_caps_lock(void);
int keyboard_num_lock(void);
int keyboard_scroll_lock(void);

#endif
