/* NexusOS neofetch — Linux-like system information with the NexusOS N logo. */
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

#define INFO_LABEL_WIDTH 16

/* ASCII version of the NexusOS logo from the supplied reference:
 * two angular pillars joined by the characteristic diagonal N stroke. */
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

static void print_logo(void) {
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    for (int i = 0; i < LOGO_LINES; ++i) {
        console_print(logo[i]);
        console_print("\n");
    }
    console_set_color(COLOR_WHITE, COLOR_BLACK);
}

static void print_label(const char *label) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    int n = 0;
    while (label[n]) { console_putchar(label[n]); n++; }
    for (; n < INFO_LABEL_WIDTH; n++) console_putchar(' ');
    console_set_color(COLOR_WHITE, COLOR_BLACK);
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

static void print_kv(const char *label, const char *value) {
    print_label(label);
    console_print(value);
    console_print("\n");
}

static void print_section(const char *name) {
    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("== ");
    console_print(name);
    console_print(" ==");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("\n");
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
    print_logo();

    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("                         N E X U S O S\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("                         custom x86_64 OS\n\n");

    print_section("SYSTEM");
    print_kv("OS", "NexusOS x86_64");
    print_kv("Version", NEXUS_VERSION_DISPLAY);
    print_kv("Kernel", "monolithic x86_64");
    print_kv("User", "root@nexusos");
    print_label("System");
    console_print(target_system_manufacturer()); console_print(" ");
    console_print(target_system_product()); console_print("\n");
    print_kv("CPU Vendor", vendor);
    print_label("CPU");
    if (have_brand) console_print(brand); else console_print("brand string unavailable");
    console_print("\n");
    print_label("CPU Speed");
    if (cpu_mhz) { console_print("~"); console_print_dec(cpu_mhz); console_print(" MHz"); }
    else console_print("unknown");
    console_print("\n");
    print_label("Logical Cores"); console_print_dec(cores); console_print("\n");
    print_label("Memory"); console_print_dec(free_mb); console_print(" MB free (boot) / ");
    console_print_dec(total_mb); console_print(" MB detected\n");
    print_label("Uptime"); console_print_dec(up_h); console_print("h ");
    console_print_dec(up_m); console_print("m "); console_print_dec(up_sec); console_print("s\n\n");

    print_section("GPU");
    print_kv("GPU", gpu ? gpu_name() : "no PCI display controller");
    print_label("GPU PCI");
    if (gpu) {
        console_print_dec(gpu->bus); console_print(":"); console_print_dec(gpu->device);
        console_print("."); console_print_dec(gpu->function); console_print(" ID ");
        console_print_hex(gpu->vendor_id); console_print(":"); console_print_hex(gpu->device_id);
    } else console_print("unavailable");
    console_print("\n");
    print_label("Display");
    if (bi && bi->fb.width && bi->fb.height) {
        console_print_dec(bi->fb.width); console_print("x"); console_print_dec(bi->fb.height);
        console_print(" @ "); console_print_dec(bi->fb.pixels_per_scanline); console_print(" pitch");
    } else console_print("unavailable");
    console_print("\n");
    print_label("Framebuffer");
    if (bi && bi->fb.base) {
        console_print_dec(fb_mb); console_print(" MB, "); print_pixel_format(bi->fb.pixel_format);
    } else console_print("unavailable");
    console_print("\n\n");

    print_section("INPUT");
    print_kv("Input", "PS/2 + USB HID");
    print_label("Keyboard"); print_status(keyboard_is_present()); console_print(" (PS/2)\n");
    print_label("Mouse"); print_status(mouse_is_present());
    if (mouse_is_present() && mouse_has_wheel()) console_print(" + wheel");
    console_print("\n");
    print_label("USB"); print_status(usb_ready);
    console_print(" / "); console_print_dec(usb_devices); console_print(" connected ports\n\n");

    print_section("STORAGE");
    print_label("Block Devices");
    console_print("detected by storage subsystem\n");
    print_label("AHCI / SATA"); print_status(ahci_is_ready()); console_print("\n");
    print_label("NVMe"); print_status(nvme_is_ready());
    if (nvme_is_ready()) {
        console_print(" / "); console_print_dec(nvme_sector_size()); console_print(" B sectors / ");
        console_print_dec(nvme_namespace_sectors()); console_print(" LBAs");
    }
    console_print("\n\n");

    print_section("PLATFORM");
    print_label("PCI Devices"); console_print_dec(pci_get_device_count()); console_print(" detected\n");
    print_label("UEFI / GOP"); print_status(bi && bi->fb.base); console_print("\n");
    print_label("SMBIOS / DMI"); print_status(bi && bi->system_info_valid); console_print("\n");
    print_label("ACPI"); print_status(bi && bi->acpi_valid);
    if (bi && bi->acpi_valid) { console_print(" rev "); console_print_dec(bi->acpi_revision); }
    console_print("\n");
    print_label("Bootloader"); console_print("custom UEFI loader\n\n");

    console_set_color(COLOR_CYAN, COLOR_BLACK);
    console_print("> NexusOS system information complete.\n\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
}
