#include "about.h"
#include "console.h"

void about_run(void) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("NexusOS\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("A hobby OS built from scratch: custom UEFI bootloader\n");
    console_print("+ custom kernel, no external SDKs.\n");
    console_print("Type 'help' to see available commands.\n");
}
