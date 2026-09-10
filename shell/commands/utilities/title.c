#include "title.h"
#include "console.h"

void title_run(char *args) {
    int start_of_word = 1;
    for (int i = 0; args[i] != '\0'; i++) {
        char c = args[i];
        if (c == ' ') {
            start_of_word = 1;
            console_putchar(c);
            continue;
        }
        if (start_of_word && c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        } else if (!start_of_word && c >= 'A' && c <= 'Z') {
            c = (char)(c - 'A' + 'a');
        }
        start_of_word = 0;
        console_putchar(c);
    }
    console_putchar('\n');
}
