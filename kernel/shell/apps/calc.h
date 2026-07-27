#ifndef NEXUSOS_CALC_H
#define NEXUSOS_CALC_H

/* Разбирает args как "<число> <оператор> <число>", где оператор один
 * из + - * /, и печатает результат. Целые числа со знаком.
 * Пример: calc_run("5 + 3") напечатает "8". */
void calc_run(const char *args);

#endif
