#include "target.h"
#include "cpu.h"
#include "pci.h"
#include "kstate.h"
#include "boot_info.h"
#include "console.h"
#include "xhci.h"
#include "ahci.h"
#include "nvme.h"
#include "gpu.h"

static int str_equal(const char *a, const char *b) {
    int i = 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == '\0' && b[i] == '\0';
}

const char *target_system_manufacturer(void) {
    nexus_boot_info_t *bi = kstate_get_boot_info();
    if (bi && bi->system_info_valid && bi->system_manufacturer[0]) return bi->system_manufacturer;
    return "Unknown";
}

const char *target_system_product(void) {
    nexus_boot_info_t *bi = kstate_get_boot_info();
    if (bi && bi->system_info_valid && bi->system_product[0]) return bi->system_product;
    return "Unknown system";
}

void target_get_status(nexus_target_status_t *status) {
    if (!status) return;
    status->intel_cpu = status->amd_cpu = 0;
    status->nvidia_gpu = status->amd_gpu = status->intel_gpu = 0;
    status->xhci = xhci_is_ready();
    status->ahci = ahci_is_ready();
    status->framebuffer = 0;
    status->pci_devices = pci_get_device_count();
    status->system_info = 0;
    status->acpi = 0;
    status->nvme = nvme_is_ready();
    status->network = 0;
    status->intel_wifi = 0;
    status->broadcom_pci = 0;

    char vendor[13];
    cpu_get_vendor(vendor);
    status->intel_cpu = str_equal(vendor, "GenuineIntel");
    status->amd_cpu = str_equal(vendor, "AuthenticAMD");

    nexus_boot_info_t *bi = kstate_get_boot_info();
    if (bi && bi->fb.base && bi->fb.width && bi->fb.height) status->framebuffer = 1;
    if (bi && bi->system_info_valid) status->system_info = 1;
    if (bi && bi->acpi_valid) status->acpi = 1;

    for (int i = 0; i < status->pci_devices; i++) {
        const nexus_pci_device_t *d = pci_get_device(i);
        if (!d) continue;
        if (d->class_code == 0x01 && d->subclass == 0x08 && d->prog_if == 0x02)
            status->nvme = 1;
        if (d->class_code == 0x02) {
            status->network = 1;
            if (d->vendor_id == 0x8086) status->intel_wifi = 1;
            if (d->vendor_id == 0x14E4) status->broadcom_pci = 1;
        }
    }

    const nexus_gpu_info_t *gpu = gpu_get_info();
    if (gpu) {
        status->nvidia_gpu = gpu->vendor == NEXUS_GPU_NVIDIA;
        status->amd_gpu = gpu->vendor == NEXUS_GPU_AMD;
        status->intel_gpu = gpu->vendor == NEXUS_GPU_INTEL;
    }
}

/* Kept for source compatibility with the old target API. A machine is not
 * accepted/rejected anymore: NexusOS adapts to what firmware reports. */
int target_profile_matches(void) { return 1; }

void target_print_profile(void) {
    nexus_target_status_t s;
    target_get_status(&s);
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("NexusOS Hardware Detection\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);

    console_print("  System:       ");
    console_print(target_system_manufacturer());
    console_print(" ");
    console_print(target_system_product());
    console_print("\n");

    console_print("  CPU:          ");
    if (s.intel_cpu) console_print("Intel x86_64");
    else if (s.amd_cpu) console_print("AMD x86_64");
    else console_print("x86_64 compatible");
    console_print("\n");

    console_print("  Graphics:     ");
    if (gpu_is_ready()) {
        console_print(gpu_vendor_name());
        console_print(" ");
        console_print(gpu_device_name());
    } else {
        console_print("not identified");
    }
    console_print(" (PCI detected)\n");

    console_print("  PCI devices:  "); console_print_dec(s.pci_devices); console_print("\n");
    console_print("  xHCI USB:     "); console_print(s.xhci ? "ready\n" : "not ready\n");
    console_print("  AHCI storage: "); console_print(s.ahci ? "ready\n" : "not ready\n");
    console_print("  GOP graphics: "); console_print(s.framebuffer ? "ready\n" : "not ready\n");
    console_print("  SMBIOS/DMI:   "); console_print(s.system_info ? "available\n" : "not available\n");
    console_print("  ACPI:         "); console_print(s.acpi ? "available\n" : "not available\n");
    console_print("  NVMe:         "); console_print(s.nvme ? "controller found\n" : "not detected\n");
    console_print("  Network PCI:  "); console_print(s.network ? "device found\n" : "not detected\n");
    console_print("  Intel Wi-Fi:  "); console_print(s.intel_wifi ? "PCI device found\n" : "not detected\n");
    console_print("  Broadcom PCI: "); console_print(s.broadcom_pci ? "PCI device found\n" : "not detected\n");
    console_print("  Broadcom USB: "); console_print(s.xhci ? "USB controller ready; device enumeration pending\n" : "USB controller unavailable\n");
    console_print("\n");
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("  Hardware mode: automatic detection\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
}
