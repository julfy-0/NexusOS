#include "inputinfo.h"
#include "console.h"
#include "keyboard.h"
#include "mouse.h"

void inputinfo_run(void) {
    console_print("Input devices\n");
    console_print("  Keyboard: PS/2 / i8042\n");
    console_print("    Shift: "); console_print(keyboard_shift_down() ? "down\n" : "up\n");
    console_print("    Ctrl:  "); console_print(keyboard_ctrl_down() ? "down\n" : "up\n");
    console_print("    Alt:   "); console_print(keyboard_alt_down() ? "down\n" : "up\n");
    console_print("    Caps:  "); console_print(keyboard_caps_lock() ? "on\n" : "off\n");
    console_print("    Num:   "); console_print(keyboard_num_lock() ? "on\n" : "off\n");
    console_print("    Scroll:"); console_print(keyboard_scroll_lock() ? " on\n" : " off\n");
    console_print("  Mouse: ");
    console_print(mouse_is_present() ? "PS/2 connected\n" : "not detected\n");
    console_print("    Position: "); console_print_dec((uint32_t)mouse_get_x());
    console_print(", "); console_print_dec((uint32_t)mouse_get_y()); console_print("\n");
    console_print("    Buttons: "); console_print_hex(mouse_get_buttons()); console_print("\n");
    console_print("    Wheel: "); console_print(mouse_has_wheel() ? "yes\n" : "no\n");
}
