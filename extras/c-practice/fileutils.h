#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <stddef.h>

/* Читает весь файл в память как строку с завершающим нулём.
 * Возвращает NULL при ошибке. Длину (без учёта '\0') записывает в *out_len, если он не NULL.
 * Результат нужно освободить через free(). */
char *file_read_all(const char *path, size_t *out_len);

/* Записывает буфер data длиной len в файл, полностью перезаписывая его.
 * Возвращает 0 при успехе, -1 при ошибке. */
int file_write_all(const char *path, const void *data, size_t len);

/* Добавляет строку text в конец файла (создаёт файл, если его нет).
 * Возвращает 0 при успехе, -1 при ошибке. */
int file_append_line(const char *path, const char *text);

/* Проверяет, существует ли файл. Возвращает 1, если да, 0 если нет. */
int file_exists(const char *path);

/* Возвращает размер файла в байтах, или -1 при ошибке. */
long file_size(const char *path);

/* Копирует файл src в dst. Возвращает 0 при успехе, -1 при ошибке. */
int file_copy(const char *src, const char *dst);

/* Считает количество строк в текстовом файле. Возвращает -1 при ошибке. */
long file_count_lines(const char *path);

#endif /* FILEUTILS_H */
