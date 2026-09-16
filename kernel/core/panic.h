#ifndef NEXUS_KERNEL_PANIC_H
#define NEXUS_KERNEL_PANIC_H

/* Аварийная остановка ядра: печатает сообщение и вешает CPU в cli+hlt.
 * Используется как последняя линия защиты при необработанных исключениях. */
void panic(const char *message) __attribute__((noreturn));

/* Показывает panic-экран и держит систему в диагностическом режиме 30 секунд,
 * после чего выполняет аппаратный reboot. */
void panic_countdown_and_reboot(void) __attribute__((noreturn));

/* Critical OS Stop: terminal kernel halt state with diagnostic countdown. */
void critical_os_stop(const char *message) __attribute__((noreturn));

#endif /* NEXUS_KERNEL_PANIC_H */
