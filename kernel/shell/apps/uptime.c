#include "uptime.h"
#include "console.h"
#include "pit.h"

void uptime_run(void) {
    uint64_t total_seconds = pit_get_uptime_seconds();
    uint64_t hours = total_seconds / 3600;
    uint64_t minutes = (total_seconds % 3600) / 60;
    uint64_t seconds = total_seconds % 60;

    console_print("Uptime: ");
    console_print_dec(hours);
    console_print("h ");
    console_print_dec(minutes);
    console_print("m ");
    console_print_dec(seconds);
    console_print("s (");
    console_print_dec(pit_get_ticks());
    console_print(" timer ticks)\n");
}
