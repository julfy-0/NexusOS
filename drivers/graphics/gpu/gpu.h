#ifndef NEXUSOS_GPU_H
#define NEXUSOS_GPU_H

#include <stdint.h>
#include "pci.h"

typedef enum {
    NEXUS_GPU_UNKNOWN = 0,
    NEXUS_GPU_INTEL,
    NEXUS_GPU_AMD,
    NEXUS_GPU_NVIDIA,
    NEXUS_GPU_VMWARE,
    NEXUS_GPU_VIRTIO,
    NEXUS_GPU_QEMU
} nexus_gpu_vendor_t;

typedef struct {
    int present;
    nexus_gpu_vendor_t vendor;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint64_t bar0;
    const char *vendor_name;
    const char *device_name;
} nexus_gpu_info_t;

int gpu_init(void);
int gpu_is_ready(void);
const nexus_gpu_info_t *gpu_get_info(void);
const char *gpu_vendor_name(void);
const char *gpu_device_name(void);

/* Returns a stable human-readable name. Unknown IDs fall back to "Unknown GPU". */
const char *gpu_name(void);

#endif
