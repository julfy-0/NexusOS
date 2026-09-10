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
#include "mount.h"
#include "pit.h"
#include "pci.h"
#include "ahci.h"
#include "fat32.h"
#include "keyboard.h"
#include "gui.h"
#include "usermode.h"
#include "xhci.h"
#include "mouse.h"
#include "target.h"
#include "nexus_version.h"

void kmain(nexus_boot_info_t *boot_info) {
    console_init(&boot_info->fb);

    if (boot_info->magic != NEXUS_BOOT_MAGIC) {
        /* Даже без валидного boot_info попробуем хоть что-то показать —
         * но полагаться на fb.* в этом случае небезопасно, поэтому просто
         * останавливаемся. */
        for (;;) { __asm__ volatile ("cli; hlt"); }
    }

    kstate_set_boot_info(boot_info);

    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("NexusOS " NEXUS_VERSION_DISPLAY "\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("================================\n\n");

    console_print("Loading GDT");
    gdt_init();
    console_status_ok();

    console_print("Installing IDT and exception handlers");
    idt_init();
    console_status_ok();

    console_print("Setting up paging (own PML4/PDPT/PD, identity map)");
    paging_init(boot_info);
    console_status_ok();

    console_print("Remapping PIC (IRQ0-15 -> vectors 32-47)");
    pic_remap();
    console_status_ok();

    console_print("Starting PIT timer (100 Hz)");
    pit_init(100);
    console_status_ok();

    console_print("Initializing keyboard controller (i8042)");
    keyboard_init();
    console_status_ok();

    /* Разрешаем таймер (IRQ0) и клавиатуру (IRQ1), остальное пока маскируем */
    for (int i = 0; i < 16; i++) pic_set_mask(i, i != 0 && i != 1 && i != 12);
    console_print("Unmasking timer, keyboard and mouse IRQs");
    console_status_ok();

    console_print("Initializing PS/2 mouse");
    mouse_init();
    if (mouse_is_present()) {
        console_status_ok();
        console_print("  -> mouse connected\n");
    } else {
        console_status_warn();
        console_print("  -> no PS/2 mouse found, continuing\n");
    }

    console_print("\n");
    console_print("Scanning PCI bus");
    pci_scan();
    console_status_ok();

    console_print("Probing AHCI disk (SATA, port 0, LBA 0)");
    if (ahci_init() && fat32_mount(0)) {
        console_status_ok();
        vfs_mount("ahci0p0", "/mnt/disk0", "fat32", VFS_MOUNT_RDONLY);
        console_set_color(COLOR_CYAN, COLOR_BLACK);
        console_print("  -> FAT32 mounted at /mnt/disk0, try 'diskls'\n");
        console_set_color(COLOR_WHITE, COLOR_BLACK);
    } else {
        console_status_warn();
        console_set_color(COLOR_YELLOW, COLOR_BLACK);
        console_print("  -> no disk found, diskls/diskcat won't work, everything else is fine\n");
        console_set_color(COLOR_WHITE, COLOR_BLACK);
    }
    console_print("\n");

    console_print("Initializing USB xHCI controller");
    if (xhci_init()) {
        console_status_ok();
        console_print("  -> USB ports: ");
        console_print_dec(xhci_port_count());
        console_print(", connected: ");
        console_print_dec(xhci_connected_ports());
        console_print("\n");
        if (xhci_keyboard_present()) {
            console_set_color(COLOR_CYAN, COLOR_BLACK);
            console_print("  -> USB keyboard ready\n");
            console_set_color(COLOR_WHITE, COLOR_BLACK);
        }
    } else {
        console_status_warn();
        console_print("  -> no xHCI controller found or initialization failed\n");
    }

    console_print("Detecting system hardware");
    console_status_ok();
    console_print("  -> automatic hardware detection enabled\n");
    nexus_target_status_t hw;
    target_get_status(&hw);
    console_print("  -> PCI network devices: "); console_print_dec(hw.network); console_print("\n");
    console_print("  -> NVMe controllers: "); console_print_dec(hw.nvme); console_print("\n");
    console_print("  -> ACPI: "); console_print(hw.acpi ? "available\n" : "not available\n");
    console_print("\n");

    /*
     * NexusOS now always enters the command line after driver
     * initialization. The graphical desktop remains available, but is
     * launched explicitly by the "desktop-run" shell command.
     */
    shell_init();
    usermode_init();
    gui_init(&boot_info->fb);

    __asm__ volatile ("sti");

    for (;;) {
        if (gui_is_active()) gui_update();
        __asm__ volatile ("hlt");
    }
}
