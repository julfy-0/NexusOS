#ifndef NEXUSOS_DISKCAT_H
#define NEXUSOS_DISKCAT_H

/* args — путь к файлу на реальном FAT32-диске. Печатает содержимое как
 * текст (для бинарников результат будет "грязным" — это ожидаемо, у нас
 * нет hex-режима вывода, только сырая печать байт как символов). */
void diskcat_run(const char *args);

#endif
