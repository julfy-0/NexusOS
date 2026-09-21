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
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "pic.h"
#include "kstate.h"
#include "shell.h"
#include "mount.h"
#include "pit.h"
#include "pci.h"
#include "ahci.h"
#include "nvme.h"
#include "fat32.h"
#include "keyboard.h"
#include "gui.h"
#include "usermode.h"
#include "process.h"
#include "syscall.h"
#include "xhci.h"
#include "gpu.h"
#include "usb.h"
#include "mouse.h"
#include "input.h"
#include "kernel_events.h"
#include "event_queue.h"
#include "scheduler.h"
#include "target.h"
#include "nexus_version.h"
#include "session.h"
#include "power.h"
#include "system_info.h"
#include "app_manager.h"
#include "package_manager.h"
#include "network.h"
#include "module.h"
#include "watchdog.h"
#include "heap.h"
#include "panic.h"

static volatile uint64_t g_scheduler_service_ticks;

static void scheduler_service_thread(void *arg) {
    (void)arg;
    for (;;) {
        /* Demonstrate process-safe event waiting: this thread sleeps until
         * normal kernel event processing observes a keyboard scancode. */
        if (kernel_events_wait(NEXUS_EVENT_KEYBOARD_SCANCODE)) {
            g_scheduler_service_ticks++;
        }
    }
}

void kmain(nexus_boot_info_t *boot_info) {
    /* The loader contract is intentionally checked before dereferencing any
     * framebuffer or memory-map field. From this point onward interrupts stay
     * disabled until the hardware initialization is complete. */
    __asm__ volatile ("cli" ::: "memory");

    if (boot_info == 0 || boot_info->magic != NEXUS_BOOT_MAGIC ||
        boot_info->fb.base == 0 || boot_info->fb.width == 0 ||
        boot_info->fb.height == 0 || boot_info->kernel_phys_end <= boot_info->kernel_phys_base ||
        boot_info->kernel_entry == 0) {
        for (;;) { __asm__ volatile ("cli; hlt"); }
    }

    console_init(&boot_info->fb);
    kstate_set_boot_info(boot_info);

    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("NexusOS " NEXUS_VERSION_DISPLAY "\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("================================\n\n");

    gdt_init();
    console_component_status("GDT", "service", "1.0", 1);

    idt_init();
    console_component_status("IDT", "service", "1.0", 1);

    paging_init(boot_info);
    console_component_status("kernel address space", "service", "1.0", 1);

    pmm_init(boot_info);
    console_component_status("physical memory manager", "service", "1.0", pmm_total_pages() != 0);

    vmm_init();
    console_component_status("virtual memory manager", "service", "1.0", 1);

    heap_init();
    console_component_status("virtual arena / kernel heap", "service", "1.0", heap_is_ready());

    if (heap_is_ready()) {
        void *heap_test = kmalloc(64);
        int allocator_ok = heap_test != NULL;
        if (allocator_ok) kfree(heap_test);
        console_component_status("kernel allocator", "service", "1.0", allocator_ok);
    } else {
        console_component_status("kernel allocator", "service", "1.0", 0);
    }

    kernel_events_init();
    console_component_status("event queue", "service", "1.0", event_queue_is_ready());

    scheduler_init(pit_get_frequency_hz());
    nexus_watchdog_init(pit_get_frequency_hz());
    console_component_status("scheduler", "service", "1.0", scheduler_is_ready());
    console_component_status("watchdog", "service", "1.0", 1);
    if (scheduler_is_ready()) {
        uint64_t service_tid = thread_create(scheduler_service_thread, NULL);
        console_component_status("scheduler service thread", "service", "1.0", service_tid != 0);
    } else {
        console_component_status("scheduler service thread", "service", "1.0", 0);
    }

    for (int i = 0; i < 16; i++) pic_set_mask(i, i != 0 && i != 1 && i != 12);
    console_component_status("interrupt routing", "service", "1.0", 1);

    module_manager_init();
    (void)module_load_all();

    /* Normal boot output intentionally stays compact. Detailed hardware
     * diagnostics are available from the corresponding subsystems. */
    if (!module_is_loaded("ahci")) {
        /* Keep storage optional; no boot failure is required here. */
    } else if (fat32_mount(0)) {
        vfs_mount("ahci0p0", "/mnt/disk0", "fat32", 0);
    }

    /*
     * NexusOS now always enters the command line after driver
     * initialization. The graphical desktop remains available, but is
     * launched explicitly by the "desktop-run" shell command.
     */
    process_init();
    console_component_status("process", "service", "1.0", process_is_ready());

    int services_ok = nexus_session_init() && nexus_power_init() && nexus_system_info_init() &&
                      nexus_app_manager_init() && nexus_package_manager_init();
    if (services_ok) {
        nexus_app_register_builtin("nexus.files", "Files", "builtin:files");
        nexus_app_register_builtin("nexus.terminal", "Terminal", "builtin:terminal");
        nexus_app_register_builtin("nexus.settings", "Settings", "builtin:settings");
        (void)nexus_package_discover("/system/apps");
        (void)nexus_package_discover("/userdata/apps");
    }
    console_component_status("system services", "service", "1.0", services_ok);

    /* Finish all startup/service initialization before handing the console
     * to the interactive shell. shell_init() intentionally clears the visible
     * boot log so startup status lines can never land inside the input prompt. */
    usermode_init();
    console_component_status("usermode", "service", "1.0", 1);
    syscall_init();
    console_component_status("syscall", "service", "1.0", 1);
    gui_init(&boot_info->fb);
    console_component_status("gui", "service", "1.0", 1);
    console_component_status("shell", "service", "1.0", 1);

    shell_init();

    __asm__ volatile ("sti");

    for (;;) {
        nexus_watchdog_heartbeat();
        if (!heap_validate()) {
            critical_os_stop("kernel heap integrity check failed");
        }
        if (nexus_watchdog_trip_pending()) {
            nexus_watchdog_clear_trip();
            critical_os_stop("normal kernel execution stalled for more than 3 seconds");
        }

        /* All keyboard/mouse/timer work captured by IRQ handlers is drained
         * here in normal kernel context, with interrupts enabled. This is the
         * event-queue boundary: shell commands are no longer
         * executed from an interrupt handler. */
        kernel_events_process();
        /* Shell commands are deliberately deferred until after the input/event
         * drain. This prevents a command from blocking the keyboard/xHCI event
         * path and makes command execution a separate normal-context phase. */
        if (!gui_is_active()) {
            shell_process_pending();
        }
        if (gui_is_active()) {
            console_cursor_disable();
            gui_update();
        } else {
            console_cursor_tick();
        }
        __asm__ volatile ("hlt");
    }
}
