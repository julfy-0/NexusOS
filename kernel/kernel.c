/* NexusOS kernel — kmain.
 *
 * На входе: boot services уже мертвы, framebuffer доступен напрямую,
 * paging — тот, что оставила прошивка (identity-map). Своя MMU-настройка —
 * следующий милстоун. */
#include <stdint.h>
#include "boot_info.h"
#include "console.h"
#include "gdt.h"
#include "idt.h"
#include "paging.h"
#include "pic.h"
#include "kstate.h"
#include "shell.h"
#include "pit.h"
#include "pci.h"
#include "ahci.h"
#include "fat32.h"
#include "keyboard.h"

static void print_banner(nexus_boot_info_t *bi) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("NexusOS\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("========\n\n");

    console_print("Kernel is alive.\n\n");

    console_print("Framebuffer: ");
    console_print_dec(bi->fb.width);
    console_print(" x ");
    console_print_dec(bi->fb.height);
    console_print(" @ ");
    console_print_hex(bi->fb.base);
    console_print("\n");

    console_print("Memory map: ");
    console_print_dec(bi->mmap.map_size / bi->mmap.descriptor_size);
    console_print(" entries, ");
    console_print_dec(bi->mmap.map_size);
    console_print(" bytes total\n\n");

    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("GDT ... ");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
}

void kmain(nexus_boot_info_t *boot_info) {
    console_init(&boot_info->fb);

    if (boot_info->magic != NEXUS_BOOT_MAGIC) {
        /* Даже без валидного boot_info попробуем хоть что-то показать —
         * но полагаться на fb.* в этом случае небезопасно, поэтому просто
         * останавливаемся. */
        for (;;) { __asm__ volatile ("cli; hlt"); }
    }

    print_banner(boot_info);
    console_print("OK\n");

    kstate_set_boot_info(boot_info);

    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("GDT loading ... ");
    gdt_init();
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("OK\n");

    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("IDT + exception handlers ... ");
    idt_init();
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("OK\n");

    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("Paging (own PML4/PDPT/PD, identity map) ... ");
    paging_init(boot_info);
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("OK\n");

    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("PIC remap (IRQ0-15 -> vectors 32-47) ... ");
    pic_remap();

    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("PIT timer (100 Hz) ... ");
    pit_init(100);
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("OK\n");

    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("Keyboard controller (i8042 init) ... ");
    keyboard_init();
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("OK\n");

    /* Разрешаем таймер (IRQ0) и клавиатуру (IRQ1), остальное пока маскируем */
    for (int i = 0; i < 16; i++) pic_set_mask(i, i != 0 && i != 1);
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("OK\n\n");

    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("AHCI disk (SATA, port 0, LBA 0) ... ");
    if (ahci_init() && fat32_mount(0)) {
        console_set_color(COLOR_GREEN, COLOR_BLACK);
        console_print("OK (FAT32 mounted, try 'diskls')\n");
    } else {
        console_set_color(COLOR_YELLOW, COLOR_BLACK);
        console_print("not found (diskls/diskcat won't work, everything else is fine)\n");
    }
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("\n");

    console_set_color(COLOR_YELLOW, COLOR_BLACK);
    console_print("\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);

    shell_init();

    __asm__ volatile ("sti");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
