#include "free.h"
#include "console.h"
#include "pmm.h"

void free_run(void) {
    uint64_t t = pmm_total_pages() * 4096ULL;
    uint64_t f = pmm_free_pages() * 4096ULL;
    uint64_t u = pmm_used_pages() * 4096ULL;
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("Memory\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("  total: "); console_print_dec(t / (1024ULL*1024ULL)); console_print(" MiB\n");
    console_print("  used:  "); console_print_dec(u / (1024ULL*1024ULL)); console_print(" MiB\n");
    console_print("  free:  "); console_print_dec(f / (1024ULL*1024ULL)); console_print(" MiB\n");
}
