#ifndef NEXUSOS_CPU_H
#define NEXUSOS_CPU_H

#include <stdint.h>

/* out должен быть минимум 13 байт (12 символов + '\0') */
void cpu_get_vendor(char *out);

/* Есть ли расширенные листья CPUID для строки модели ("brand string")? */
int cpu_has_brand_string(void);

/* out должен быть минимум 49 байт (48 символов + '\0'). Вызывать только
 * если cpu_has_brand_string() вернула ненулевое значение. */
void cpu_get_brand(char *out);

/* Грубая оценка числа логических ядер (устаревшее поле CPUID.01h:EBX[23:16],
 * не всегда точное на современных многосокетных системах, но для
 * демонстрации достаточно). */
uint32_t cpu_logical_cores(void);

#endif
