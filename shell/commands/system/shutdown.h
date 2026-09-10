#ifndef NEXUSOS_SHUTDOWN_H
#define NEXUSOS_SHUTDOWN_H
/* Пытается выключить машину через ACPI-трюк QEMU (порт 0x604).
 * Специфично для QEMU/Bochs — на реальном железе или в других версиях
 * QEMU может не сработать, тогда откатывается на halt. */
void shutdown_run(void);
#endif
