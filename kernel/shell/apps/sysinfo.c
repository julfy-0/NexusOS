#include "sysinfo.h"
#include "console.h"

void sysinfo_run(void) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("NexusOS shell status\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("  buffer-based line input, no processes yet\n");
    console_print("  commands run synchronously inside keyboard IRQ\n");
}