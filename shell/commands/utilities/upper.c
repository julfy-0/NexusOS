#include "upper.h"
#include "console.h"

void upper_run(char *args) {
    for (int i = 0; args[i] != '\0'; i++) {
        char c = args[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        console_putchar(c);
    }
    console_putchar('\n');
}
