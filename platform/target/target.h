#ifndef NEXUSOS_TARGET_H
#define NEXUSOS_TARGET_H

#include <stdint.h>

/* Generic runtime hardware inventory. No laptop/desktop model is hard-coded. */
typedef struct {
    int intel_cpu;
    int amd_cpu;
    int nvidia_gpu;
    int amd_gpu;
    int intel_gpu;
    int xhci;
    int ahci;
    int framebuffer;
    int pci_devices;
    int system_info;
    int acpi;
    int nvme;
    int network;
    int intel_wifi;
    int broadcom_pci;
} nexus_target_status_t;

void target_get_status(nexus_target_status_t *status);
int target_profile_matches(void);
void target_print_profile(void);
const char *target_system_manufacturer(void);
const char *target_system_product(void);

#endif
