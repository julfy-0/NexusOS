/* NexusOS neofetch — compact side-by-side system summary with the NexusOS logo. */
#include "neofetch.h"
#include "console.h"
#include "cpu.h"
#include "pit.h"
#include "kstate.h"
#include "boot_info.h"
#include "nexus_version.h"
#include "target.h"
#include "gpu.h"
#include "pci.h"
#include "keyboard.h"
#include "mouse.h"
#include "xhci.h"
#include "uhci.h"
#include "ohci.h"
#include "ehci.h"
#include "ahci.h"
#include "nvme.h"

#define LOGO_WIDTH 31
#define GAP "     "

/* The NexusOS N mark used by the shell. It is kept as ASCII so neofetch works
 * on the framebuffer console without requiring image loading or a GUI. */
static const char *logo[] = {
    "        ##                  ##",
    "        ###                 ##",
    "        ####                ##",
    "        #####               ##",
    "        ######              ##",
    "        ##  ###             ##",
    "        ##   ###            ##",
    "        ##    ###           ##",
    "        ##     ###          ##",
    "        ##      ###         ##",
    "        ##       ###        ##",
    "        ##        ###       ##",
    "        ##         ###      ##",
    "        ##          ####   ##",
    "        ##              ####",
    "        ##                ##",
};
#define LOGO_LINES ((int)(sizeof(logo) / sizeof(logo[0])))

static void print_spaces(int count) {
    while (count-- > 0) console_putchar(' ');
}

static int text_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void print_logo_part(int line) {
    if (line < LOGO_LINES) {
        console_set_color(COLOR_CYAN, COLOR_BLACK);
        console_print(logo[line]);
        print_spaces(LOGO_WIDTH - 1 - text_len(logo[line]));
    } else {
        print_spaces(LOGO_WIDTH - 1);
    }
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print(GAP);
}

static void print_label(const char *label) {
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print(label);
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print(": ");
}

static void print_status(int ready) {
    console_set_color(ready ? COLOR_GREEN : COLOR_RED, COLOR_BLACK);
    console_print(ready ? "ready" : "not ready");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
}

static void print_pixel_format(uint32_t format) {
    switch (format) {
        case NEXUS_PIXFMT_RGB: console_print("RGB"); break;
        case NEXUS_PIXFMT_BGR: console_print("BGR"); break;
        default: console_print("other"); break;
    }
}

static void print_rule(void) {
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("----------------------------");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
}

void neofetch_run(void) {
    nexus_boot_info_t *bi = kstate_get_boot_info();
    const nexus_gpu_info_t *gpu = gpu_get_info();

    char vendor[13];
    cpu_get_vendor(vendor);

    char brand[49];
    int have_brand = cpu_has_brand_string();
    if (have_brand) cpu_get_brand(brand);

    uint32_t cores = cpu_logical_cores();
    uint32_t cpu_mhz = cpu_measure_freq_mhz();

    uint64_t total_pages = 0, conventional_pages = 0;
    kstate_mem_summary(&total_pages, &conventional_pages);
    uint64_t total_mb = (total_pages * 4096ULL) / (1024ULL * 1024ULL);
    uint64_t free_mb = (conventional_pages * 4096ULL) / (1024ULL * 1024ULL);

    uint64_t uptime_s = pit_get_uptime_seconds();
    uint64_t up_h = uptime_s / 3600;
    uint64_t up_m = (uptime_s % 3600) / 60;
    uint64_t up_sec = uptime_s % 60;

    uint64_t fb_mb = 0;
    if (bi && bi->fb.size) fb_mb = bi->fb.size / (1024ULL * 1024ULL);

    int usb_ready = xhci_is_ready() || ehci_is_ready() || ohci_is_ready() || uhci_is_ready();
    int usb_devices = (int)xhci_connected_ports() + (int)ehci_connected_ports() +
                      (int)ohci_connected_ports() + (int)uhci_connected_ports();

    console_print("\n");

    /* Print the logo and system information on the same rows, like neofetch. */
    for (int line = 0; line < 28; ++line) {
        print_logo_part(line);

        switch (line) {
            case 0:
                console_set_color(COLOR_CYAN, COLOR_BLACK);
                console_print("root@nexusos");
                console_set_color(COLOR_WHITE, COLOR_BLACK);
                break;
            case 1:
                print_rule();
                break;
            case 2:
                print_label("OS"); console_print("NexusOS x86_64");
                break;
            case 3:
                print_label("Version"); console_print(NEXUS_VERSION_DISPLAY);
                break;
            case 4:
                print_label("Kernel"); console_print("monolithic x86_64");
                break;
            case 5:
                print_label("User"); console_print("root@nexusos");
                break;
            case 6:
                print_label("System"); console_print(target_system_manufacturer());
                console_putchar(' '); console_print(target_system_product());
                break;
            case 7:
                print_label("CPU Vendor"); console_print(vendor);
                break;
            case 8:
                print_label("CPU");
                if (have_brand) console_print(brand); else console_print("brand string unavailable");
                break;
            case 9:
                print_label("CPU Speed");
                if (cpu_mhz) { console_putchar('~'); console_print_dec(cpu_mhz); console_print(" MHz"); }
                else console_print("unknown");
                break;
            case 10:
                print_label("Logical Cores"); console_print_dec(cores);
                break;
            case 11:
                print_label("Memory"); console_print_dec(free_mb); console_print(" MB free / ");
                console_print_dec(total_mb); console_print(" MB detected");
                break;
            case 12:
                print_label("Uptime"); console_print_dec(up_h); console_print("h ");
                console_print_dec(up_m); console_print("m "); console_print_dec(up_sec); console_print("s");
                break;
            case 13:
                print_label("GPU"); console_print(gpu ? gpu_name() : "no PCI display controller");
                break;
            case 14:
                print_label("Display");
                if (bi && bi->fb.width && bi->fb.height) {
                    console_print_dec(bi->fb.width); console_putchar('x'); console_print_dec(bi->fb.height);
                    console_print(" @ "); console_print_dec(bi->fb.pixels_per_scanline); console_print(" pitch");
                } else console_print("unavailable");
                break;
            case 15:
                print_label("Framebuffer");
                if (bi && bi->fb.base) {
                    console_print_dec(fb_mb); console_print(" MB, "); print_pixel_format(bi->fb.pixel_format);
                } else console_print("unavailable");
                break;
            case 16:
                print_label("Input"); console_print("PS/2 + USB HID");
                break;
            case 17:
                print_label("Keyboard"); print_status(keyboard_is_present()); console_print(" (PS/2)");
                break;
            case 18:
                print_label("Mouse"); print_status(mouse_is_present());
                if (mouse_is_present() && mouse_has_wheel()) console_print(" + wheel");
                break;
            case 19:
                print_label("USB"); print_status(usb_ready); console_print(" / ");
                console_print_dec(usb_devices); console_print(" connected ports");
                break;
            case 20:
                print_label("AHCI / SATA"); print_status(ahci_is_ready());
                break;
            case 21:
                print_label("NVMe"); print_status(nvme_is_ready());
                if (nvme_is_ready()) {
                    console_print(" / "); console_print_dec(nvme_sector_size()); console_print(" B sectors / ");
                    console_print_dec(nvme_namespace_sectors()); console_print(" LBAs");
                }
                break;
            case 22:
                print_label("PCI Devices"); console_print_dec(pci_get_device_count()); console_print(" detected");
                break;
            case 23:
                print_label("UEFI / GOP"); print_status(bi && bi->fb.base);
                break;
            case 24:
                print_label("SMBIOS / DMI"); print_status(bi && bi->system_info_valid);
                break;
            case 25:
                print_label("ACPI"); print_status(bi && bi->acpi_valid);
                if (bi && bi->acpi_valid) { console_print(" rev "); console_print_dec(bi->acpi_revision); }
                break;
            case 26:
                print_label("Bootloader"); console_print("custom UEFI loader");
                break;
            case 27:
                console_set_color(COLOR_CYAN, COLOR_BLACK); console_print("  ");
                console_set_color(COLOR_WHITE, COLOR_BLACK); console_print("  ");
                console_set_color(COLOR_GREEN, COLOR_BLACK); console_print("  ");
                console_set_color(COLOR_YELLOW, COLOR_BLACK); console_print("  ");
                console_set_color(COLOR_RED, COLOR_BLACK); console_print("  ");
                console_set_color(COLOR_WHITE, COLOR_BLACK);
                break;
        }
        console_print("\n");
    }
    console_print("\n");
}
