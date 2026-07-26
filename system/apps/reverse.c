#include "reverse.h"
#include "console.h"

#define REVERSE_MAX_LEN 255

void reverse_run(const char *args) {
    /* Находим длину строки сами (strlen может быть недоступен) */
    int len = 0;
    while (args[len] != '\0' && len < REVERSE_MAX_LEN) {
        len++;
    }

    for (int i = len - 1; i >= 0; i--) {
        console_putchar(args[i]);
    }
    console_putchar('\n');
}
