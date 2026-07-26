#ifndef MATHUTILS_H
#define MATHUTILS_H

#include <stddef.h>

/* Наибольший общий делитель. */
long gcd(long a, long b);

/* Наименьшее общее кратное. */
long lcm(long a, long b);

/* Проверка на простое число. */
int is_prime(long n);

/* Быстрое возведение в степень по модулю: (base^exp) mod mod. */
long pow_mod(long base, long exp, long mod);

/* Среднее арифметическое массива из n значений double. */
double mean(const double *values, size_t n);

/* Стандартное отклонение (несмещённое, population) массива из n значений double. */
double stddev(const double *values, size_t n);

/* Находит минимум в массиве double. */
double min_value(const double *values, size_t n);

/* Находит максимум в массиве double. */
double max_value(const double *values, size_t n);

/* Ограничивает значение value диапазоном [lo, hi]. */
double clamp(double value, double lo, double hi);

/* Линейная интерполяция между a и b с коэффициентом t (обычно 0..1). */
double lerp(double a, double b, double t);

#endif /* MATHUTILS_H */
