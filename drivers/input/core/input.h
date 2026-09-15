#ifndef NEXUSOS_INPUT_H
#define NEXUSOS_INPUT_H

#include <stdint.h>

typedef enum {
    NEXUS_INPUT_SOURCE_PS2 = 1,
    NEXUS_INPUT_SOURCE_USB = 2
} nexus_input_source_t;

typedef enum {
    NEXUS_INPUT_DEVICE_KEYBOARD = 1,
    NEXUS_INPUT_DEVICE_MOUSE = 2
} nexus_input_device_t;

void input_init(void);
void input_record_keyboard(nexus_input_source_t source, uint8_t value);
void input_record_mouse(nexus_input_source_t source, int32_t dx, int32_t dy,
                        uint8_t buttons, int8_t wheel);
uint64_t input_keyboard_events(nexus_input_source_t source);
uint64_t input_mouse_events(nexus_input_source_t source);
int input_source_present(nexus_input_source_t source, nexus_input_device_t device);
void input_set_present(nexus_input_source_t source, nexus_input_device_t device, int present);
const char *input_source_name(nexus_input_source_t source);

#endif
