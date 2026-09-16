#include "ps.h"
#include "console.h"
#include "process.h"

static const char *state_name(process_state_t s) {
    switch (s) {
        case PROCESS_UNUSED: return "unused";
        case PROCESS_READY: return "ready";
        case PROCESS_RUNNING: return "running";
        case PROCESS_ZOMBIE: return "zombie";
        default: return "?";
    }
}

void ps_run(void) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("PID  PPID  STATE      CR3\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    for (uint64_t pid = 1; pid < 256; ++pid) {
        nexus_process_t *p = process_get(pid);
        if (!p) continue;
        console_print_dec(p->pid); console_print("  ");
        console_print_dec(p->parent_pid); console_print("   ");
        console_print(state_name(p->state)); console_print("  ");
        console_print_hex(p->address_space_cr3); console_print("\n");
    }
    console_print("processes: "); console_print_dec(process_count());
    console_print("  zombies: "); console_print_dec(process_zombie_count()); console_print("\n");
}
