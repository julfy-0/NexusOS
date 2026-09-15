#ifndef NEXUSOS_MOUSE_H
#define NEXUSOS_MOUSE_H
#include <stdint.h>

/* PS/2 auxiliary-device driver.
 *
 * Supports the standard 3-byte PS/2 packet and automatically enables the
 * IntelliMouse-compatible 4-byte packet when the device reports ID 0x03.
 * Buttons: bit0 left, bit1 right, bit2 middle, bit3/4 extra buttons.
 */
void mouse_init(void);
void mouse_handle_irq(void);
int mouse_process_byte(uint8_t byte);
int mouse_poll(void);
void mouse_set_screen_size(uint32_t width, uint32_t height);
int mouse_is_present(void);
int mouse_has_moved(void);
int32_t mouse_get_x(void);
int32_t mouse_get_y(void);
uint8_t mouse_get_buttons(void);
int8_t mouse_get_wheel(void);
int mouse_has_wheel(void);
void mouse_clear_moved(void);
void mouse_clear_wheel(void);
void mouse_process_usb_report(const uint8_t *report, uint8_t length);

#endif
