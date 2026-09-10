#include "shutdown.h"
#include "console.h"

static inline void shutdown_outw(unsigned short port, unsigned short val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

void shutdown_run(void) {
    console_print("Shutting down (QEMU ACPI trick, port 0x604)...\n");

    /* Классический трюк выключения QEMU: записать 0x2000 в порт 0x604
     * (ACPI PM1a control, дефолтный адрес в QEMU). На реальном железе
     * или в других эмуляторах может не сработать. */
    shutdown_outw(0x604, 0x2000);

    /* Если ничего не произошло — откатываемся на обычный halt, чтобы не
     * продолжать работу в неопределённом состоянии. */
    console_print("ACPI shutdown did not work here, halting instead.\n");
    __asm__ volatile ("cli");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
