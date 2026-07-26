#include "lower.h"
#include "console.h"

void lower_run(char *args) {
    for (int i = 0; args[i] != '\0'; i++) {
        char c = args[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        console_putchar(c);
    }
    console_putchar('\n');
}
