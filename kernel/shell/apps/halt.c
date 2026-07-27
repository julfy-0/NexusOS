#include "halt.h"
#include "console.h"

void halt_run(void) {
    console_print("System halted. It is now safe to close the emulator.\n");
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
