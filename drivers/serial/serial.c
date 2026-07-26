/*
 * NexusOS — драйвер последовательного порта COM1 (0x3F8).
 * Полезен для лога, который можно снять с QEMU (-serial stdio),
 * даже если что-то сломалось на VGA-экране.
 */
#include "serial.h"
#include <arch/i386/io.h>
#include <nexus/types.h>

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);    /* отключаем прерывания */
    outb(COM1 + 3, 0x80);    /* включаем DLAB для установки скорости */
    outb(COM1 + 0, 0x03);    /* делитель = 3 -> 38400 бод (lo byte) */
    outb(COM1 + 1, 0x00);    /*                                (hi byte) */
    outb(COM1 + 3, 0x03);    /* 8 бит, без чётности, 1 стоп-бит */
    outb(COM1 + 2, 0xC7);    /* включаем FIFO, чистим его, порог 14 байт */
    outb(COM1 + 4, 0x0B);    /* IRQ включены, RTS/DSR установлены */
}

static int serial_is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c) {
    while (!serial_is_transmit_empty());
    outb(COM1, (u8)c);
}

void serial_puts(const char *s) {
    while (*s) {
        serial_putchar(*s++);
    }
}
