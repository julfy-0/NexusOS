#ifndef NEXUSOS_MOUSE_H
#define NEXUSOS_MOUSE_H
#include <stdint.h>

void mouse_init(void);
void mouse_handle_irq(void);
/* Poll the PS/2 auxiliary port without consuming keyboard bytes.
 * Returns non-zero when a complete mouse packet was processed. */
int mouse_poll(void);
void mouse_set_screen_size(uint32_t width, uint32_t height);
int mouse_is_present(void);
int mouse_has_moved(void);
int32_t mouse_get_x(void);
int32_t mouse_get_y(void);
uint8_t mouse_get_buttons(void);
void mouse_clear_moved(void);

#endif
