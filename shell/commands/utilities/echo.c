#include "echo.h"
#include "console.h"

void echo_run(const char *args) {
    console_print(args);
    console_putchar('\n');
}
