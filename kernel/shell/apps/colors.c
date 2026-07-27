#include "colors.h"
#include "console.h"

typedef struct {
    int color;
    const char *name;
} color_entry_t;

void colors_run(void) {
    static const color_entry_t table[] = {
        { COLOR_WHITE, "WHITE" },
        { COLOR_CYAN, "CYAN" },
        { COLOR_YELLOW, "YELLOW" },
        { COLOR_GREEN, "GREEN" },
    };
    const int count = 4;

    for (int i = 0; i < count; i++) {
        console_set_color(table[i].color, COLOR_BLACK);
        console_print(table[i].name);
        console_print("\n");
    }

    console_set_color(COLOR_WHITE, COLOR_BLACK);
}
