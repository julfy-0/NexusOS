#ifndef NEXUSOS_BEEP_H
#define NEXUSOS_BEEP_H
/* Пищит PC speaker через PIT-канал 2 (порт 0x61) — классический трюк. */
void beep_run(void);
#endif
