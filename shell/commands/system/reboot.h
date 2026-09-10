#ifndef NEXUSOS_REBOOT_H
#define NEXUSOS_REBOOT_H

/* Перезагружает машину через контроллер 8042 (impulse на линию reset).
 * Не возвращается. */
void reboot_run(void);

#endif
