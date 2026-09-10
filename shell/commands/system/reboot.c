#include "reboot.h"
#include "console.h"

static inline void reboot_outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char reboot_inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void reboot_run(void) {
    console_print("Rebooting...\n");

    /* Ждём, пока входной буфер контроллера 8042 освободится
     * (бит 1 регистра статуса на порту 0x64). */
    unsigned char status;
    do {
        status = reboot_inb(0x64);
    } while (status & 0x02);

    /* Команда 0xFE контроллеру 8042 — импульс на линии reset CPU. */
    reboot_outb(0x64, 0xFE);

    /* Если контроллер почему-то не перезагрузил машину — виснем тут,
     * а не продолжаем работу в неопределённом состоянии. */
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
