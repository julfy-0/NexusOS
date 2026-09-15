#include "inputinfo.h"
#include "console.h"
#include "keyboard.h"
#include "mouse.h"
#include "xhci.h"
#include "input.h"

static void print_u64(uint64_t v) {
    if (v > 0xFFFFFFFFu) { console_print_dec((uint32_t)(v >> 32)); console_print(" (hi), "); }
    console_print_dec((uint32_t)v);
}

void inputinfo_run(void) {
    console_print("Unified Input\n");
    console_print("  PS/2 keyboard: "); console_print(input_source_present(NEXUS_INPUT_SOURCE_PS2, NEXUS_INPUT_DEVICE_KEYBOARD) ? "ready\n" : "not detected\n");
    console_print("  USB HID keyboard: "); console_print(xhci_keyboard_present() ? "ready\n" : "not detected\n");
    console_print("  PS/2 mouse: "); console_print(input_source_present(NEXUS_INPUT_SOURCE_PS2, NEXUS_INPUT_DEVICE_MOUSE) ? "ready\n" : "not detected\n");
    console_print("  USB HID mouse: "); console_print(xhci_mouse_present() ? "ready\n" : "not detected\n");
    console_print("  Keyboard events PS/2: "); print_u64(input_keyboard_events(NEXUS_INPUT_SOURCE_PS2)); console_print("\n");
    console_print("  Keyboard events USB: "); print_u64(input_keyboard_events(NEXUS_INPUT_SOURCE_USB)); console_print("\n");
    console_print("  Mouse events PS/2: "); print_u64(input_mouse_events(NEXUS_INPUT_SOURCE_PS2)); console_print("\n");
    console_print("  Mouse events USB: "); print_u64(input_mouse_events(NEXUS_INPUT_SOURCE_USB)); console_print("\n");
    console_print("  Position: "); console_print_dec((uint32_t)mouse_get_x()); console_print(", "); console_print_dec((uint32_t)mouse_get_y()); console_print("\n");
    console_print("  Buttons: "); console_print_hex(mouse_get_buttons()); console_print("\n");
    console_print("  Wheel: "); console_print(mouse_has_wheel() ? "yes\n" : "no\n");
    console_print("  Modifiers: shift="); console_print(keyboard_shift_down() ? "down" : "up");
    console_print(" ctrl="); console_print(keyboard_ctrl_down() ? "down" : "up");
    console_print(" alt="); console_print(keyboard_alt_down() ? "down\n" : "up\n");
}
