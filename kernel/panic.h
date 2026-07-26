#ifndef NEXUS_KERNEL_PANIC_H
#define NEXUS_KERNEL_PANIC_H

/* Аварийная остановка ядра: печатает сообщение и вешает CPU в cli+hlt.
 * Используется как последняя линия защиты при необработанных исключениях. */
void panic(const char *message) __attribute__((noreturn));

#endif /* NEXUS_KERNEL_PANIC_H */
