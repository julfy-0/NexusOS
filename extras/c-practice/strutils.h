#ifndef STRUTILS_H
#define STRUTILS_H

#include <stddef.h>

/* Удаляет пробельные символы в начале и конце строки (изменяет строку на месте).
 * Возвращает указатель на начало обрезанной строки. */
char *str_trim(char *s);

/* Проверяет, начинается ли строка s с префикса prefix. */
int str_starts_with(const char *s, const char *prefix);

/* Проверяет, заканчивается ли строка s суффиксом suffix. */
int str_ends_with(const char *s, const char *suffix);

/* Разбивает строку по разделителю delim.
 * Возвращает массив строк (динамически выделенный), количество записывает в *count.
 * Освобождать результат нужно через str_split_free(). */
char **str_split(const char *s, char delim, size_t *count);

/* Освобождает память, выделенную str_split(). */
void str_split_free(char **parts, size_t count);

/* Заменяет все вхождения подстроки from на to в строке s.
 * Возвращает новую строку (нужно освободить через free()). */
char *str_replace(const char *s, const char *from, const char *to);

/* Переводит строку в нижний регистр (изменяет на месте). */
void str_tolower(char *s);

/* Переводит строку в верхний регистр (изменяет на месте). */
void str_toupper(char *s);

/* Проверяет, является ли строка палиндромом (без учёта регистра и пробелов). */
int str_is_palindrome(const char *s);

/* Дублирует строку (аналог strdup, но переносимо). */
char *str_dup(const char *s);

#endif /* STRUTILS_H */
