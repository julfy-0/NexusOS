#include "meminfo.h"
#include "console.h"
#include "paging.h"
#include "pmm.h"
#include "kstate.h"
#include "heap.h"
#include "scheduler.h"
#include "kernel_events.h"

static void print_mib(uint64_t pages) {
    console_print_dec((pages * 4096ULL) / (1024ULL * 1024ULL));
    console_print(" MiB");
}

void meminfo_run(void) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("Memory\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);

    console_print("  Managed physical: ");
    print_mib(pmm_total_pages());
    console_print("\n");
    console_print("  Free physical:    ");
    print_mib(pmm_free_pages());
    console_print("\n");
    console_print("  Used physical:    ");
    print_mib(pmm_used_pages());
    console_print("\n");
    console_print("  Page size:        4096 bytes\n");
    console_print("  Max physical:     ");
    console_print_hex(pmm_max_physical_address());
    console_print("\n");
    console_print("  PMM bitmap:       ");
    console_print_dec(pmm_bitmap_bytes());
    console_print(" bytes\n\n");

    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("Paging\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("  CR3 (PML4 phys):   ");
    console_print_hex(paging_get_cr3());
    console_print("\n  Own page tables:   yes\n");
    console_print("  Identity map:      2 MiB pages\n");
    console_print("  Base identity map: 0 .. ");
    console_print_dec(paging_base_identity_gib());
    console_print(" GiB\n\n");

    uint64_t total_pages = 0, conventional_pages = 0;
    kstate_mem_summary(&total_pages, &conventional_pages);

    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("EFI memory map\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("  Total described:   ");
    console_print_dec((total_pages * 4096ULL) / (1024ULL * 1024ULL));
    console_print(" MiB\n");
    console_print("  Conventional boot: ");
    console_print_dec((conventional_pages * 4096ULL) / (1024ULL * 1024ULL));
    console_print(" MiB\n\n");

    console_print("PMM status:         active\n");
    console_print("Allocation:         4 KiB physical pages\n\n");

    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("Scheduler\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("  Status:            ");
    console_print(scheduler_is_ready() ? "ready\n" : "disabled\n");
    console_print("  Timer:             ");
    console_print_dec(scheduler_timer_hz());
    console_print(" Hz\n");
    console_print("  Quantum:           ");
    console_print_dec(scheduler_quantum_ticks());
    console_print(" ticks\n");
    console_print("  Scheduler ticks:   ");
    console_print_dec(scheduler_ticks());
    console_print("\n");
    console_print("  Quantum expirations:");
    console_print_dec(scheduler_quantum_expirations());
    console_print("\n");
    console_print("  Context switches:  ");
    console_print_dec(scheduler_context_switches());
    console_print("\n");
    console_print("  Threads:           ");
    console_print_dec(scheduler_thread_count());
    console_print(" / ");
    console_print_dec(scheduler_max_threads());
    console_print("\n  Ready queue:       ");
    console_print_dec(scheduler_ready_count());
    console_print("\n  Sleeping:          ");
    console_print_dec(scheduler_sleeping_count());
    console_print("\n  Current TID:       ");
    console_print_dec(scheduler_current_thread_id());
    console_print("\n");

    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("Kernel events\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("  Processed:         ");
    console_print_dec(kernel_events_processed());
    console_print("\n");
    console_print("  Keyboard sequence: ");
    console_print_dec(kernel_events_sequence(NEXUS_EVENT_KEYBOARD_SCANCODE));
    console_print(" | waiters: ");
    console_print_dec(kernel_events_waiter_count(NEXUS_EVENT_KEYBOARD_SCANCODE));
    console_print("\n");
    console_print("  Mouse sequence:    ");
    console_print_dec(kernel_events_sequence(NEXUS_EVENT_MOUSE_BYTE));
    console_print(" | waiters: ");
    console_print_dec(kernel_events_waiter_count(NEXUS_EVENT_MOUSE_BYTE));
    console_print("\n");
}
