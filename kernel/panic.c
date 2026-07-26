#include "panic.h"
#include <lib/stdio.h>
#include <drivers/vga/vga.h>

void panic(const char *message) {
    __asm__ volatile ("cli");

    vga_set_color(VGA_WHITE, VGA_RED);
    kprintf("\n*** NEXUSOS KERNEL PANIC ***\n%s\n"
            "System halted.\n", message);

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
