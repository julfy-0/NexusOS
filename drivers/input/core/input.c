#include "input.h"

static volatile uint64_t g_keyboard_events[3];
static volatile uint64_t g_mouse_events[3];
static volatile uint8_t g_present[3][3];

void input_init(void) {
    for (int s = 0; s < 3; ++s) {
        g_keyboard_events[s] = 0;
        g_mouse_events[s] = 0;
        for (int d = 0; d < 3; ++d) g_present[s][d] = 0;
    }
}

void input_record_keyboard(nexus_input_source_t source, uint8_t value) {
    (void)value;
    if ((uint32_t)source < 3u) g_keyboard_events[source]++;
}

void input_record_mouse(nexus_input_source_t source, int32_t dx, int32_t dy,
                        uint8_t buttons, int8_t wheel) {
    (void)dx; (void)dy; (void)buttons; (void)wheel;
    if ((uint32_t)source < 3u) g_mouse_events[source]++;
}

uint64_t input_keyboard_events(nexus_input_source_t source) {
    return ((uint32_t)source < 3u) ? g_keyboard_events[source] : 0;
}
uint64_t input_mouse_events(nexus_input_source_t source) {
    return ((uint32_t)source < 3u) ? g_mouse_events[source] : 0;
}
int input_source_present(nexus_input_source_t source, nexus_input_device_t device) {
    return ((uint32_t)source < 3u && (uint32_t)device < 3u) ? g_present[source][device] != 0 : 0;
}
void input_set_present(nexus_input_source_t source, nexus_input_device_t device, int present) {
    if ((uint32_t)source < 3u && (uint32_t)device < 3u) g_present[source][device] = present ? 1 : 0;
}
const char *input_source_name(nexus_input_source_t source) {
    return source == NEXUS_INPUT_SOURCE_USB ? "USB HID" : "PS/2";
}
