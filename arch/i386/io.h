/*
 * NexusOS — низкоуровневые операции с портами x86 (in/out).
 * Инлайн-ассемблер здесь неизбежен: это единственный способ
 * выполнить инструкции IN/OUT напрямую из C.
 */
#ifndef NEXUS_ARCH_I386_IO_H
#define NEXUS_ARCH_I386_IO_H

#include <nexus/types.h>

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(u16 port, u16 value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline u16 inw(u16 port) {
    u16 ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Небольшая задержка через запись в неиспользуемый порт — стандартный
 * приём для «успокоения» старого железа между операциями с портами. */
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif /* NEXUS_ARCH_I386_IO_H */
