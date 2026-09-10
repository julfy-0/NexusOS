#ifndef NEXUSOS_SYSINFO_H
#define NEXUSOS_SYSINFO_H

/* Выводит краткую информацию об окружении шелла.
 * Не блокирует, не выделяет память — использует только console_*. */
void sysinfo_run(void);

#endif