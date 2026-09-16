#include "fontinfo.h"
#include "console.h"
#include "font_service.h"

void fontinfo_run(void) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("Font system\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("  family: "); console_print(nexus_font_family()); console_print("\n");
    console_print("  format: TrueType (.ttf)\n");
    console_print("  regular: "); console_print_dec(nexus_font_regular_size()); console_print(" bytes\n");
    console_print("  bold:    "); console_print_dec(nexus_font_bold_size()); console_print(" bytes\n");
    console_print("  service: ");
    console_set_color(nexus_font_service_ready() ? COLOR_GREEN : COLOR_RED, COLOR_BLACK);
    console_print(nexus_font_service_ready() ? "ready" : "failed");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("\n");
    console_print("  renderer: bitmap compatibility fallback; TTF assets are resident\n");
}
