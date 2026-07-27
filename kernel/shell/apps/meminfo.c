#include "meminfo.h"
#include "console.h"
#include "paging.h"
#include "kstate.h"

void meminfo_run(void) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("Paging\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("  CR3 (PML4 phys):   ");
    console_print_hex(paging_get_cr3());
    console_print("\n  Own page tables:   yes (built in mm/paging.c, not UEFI's)\n");
    console_print("  Page size:         2 MiB\n");
    console_print("  Base identity map: 0 .. ");
    console_print_dec(paging_base_identity_gib());
    console_print(" GiB (+ framebuffer + everything in the EFI memory map)\n\n");

    uint64_t total_pages = 0, conventional_pages = 0;
    kstate_mem_summary(&total_pages, &conventional_pages);

    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("EFI memory map (from boot)\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("  Total described:   ");
    console_print_dec(total_pages * 4);
    console_print(" KiB\n");
    console_print("  Conventional (free at boot): ");
    console_print_dec(conventional_pages * 4);
    console_print(" KiB\n\n");

    console_print("Not yet implemented: kmalloc/kfree, page fault beyond\n");
    console_print("diagnostics, higher-half kernel. See docs/ROADMAP.md.\n");
}
