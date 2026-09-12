#include "gpu.h"
#include "intel_gpu.h"
#include "amd_gpu.h"
#include "nvidia_gpu.h"
#include "console.h"

static nexus_gpu_info_t g_gpu;

static const char *vendor_name(uint16_t id) {
    switch (id) {
        case 0x8086: return "Intel";
        case 0x1002: return "AMD";
        case 0x10DE: return "NVIDIA";
        case 0x15AD: return "VMware";
        case 0x1AF4: return "VirtIO";
        case 0x1234: return "QEMU";
        default: return "Unknown";
    }
}

static nexus_gpu_vendor_t vendor_type(uint16_t id) {
    switch (id) {
        case 0x8086: return NEXUS_GPU_INTEL;
        case 0x1002: return NEXUS_GPU_AMD;
        case 0x10DE: return NEXUS_GPU_NVIDIA;
        case 0x15AD: return NEXUS_GPU_VMWARE;
        case 0x1AF4: return NEXUS_GPU_VIRTIO;
        case 0x1234: return NEXUS_GPU_QEMU;
        default: return NEXUS_GPU_UNKNOWN;
    }
}

static const char *device_name(uint16_t vendor, uint16_t device) {
    switch (vendor) {
        case 0x8086: return intel_gpu_device_name(device);
        case 0x1002: return amd_gpu_device_name(device);
        case 0x10DE: return nvidia_gpu_device_name(device);
        case 0x1234:
            if (device == 0x1111) return "QEMU Standard VGA";
            return "QEMU Display Adapter";
        case 0x1AF4:
            if (device == 0x1050) return "VirtIO GPU";
            return "VirtIO Display Adapter";
        case 0x15AD:
            if (device == 0x0405) return "VMware SVGA II";
            return "VMware Graphics Adapter";
        default: return "Unknown GPU";
    }
}

int gpu_init(void) {
    g_gpu.present = 0;

    /* PCI class 03 = Display Controller. Prefer a VGA-compatible controller
     * (subclass 00) but accept 3D controllers (subclass 02) as well. */
    for (int i = 0; i < pci_get_device_count(); i++) {
        const nexus_pci_device_t *d = pci_get_device(i);
        if (!d || d->class_code != 0x03) continue;
        if (d->subclass != 0x00 && d->subclass != 0x01 && d->subclass != 0x02)
            continue;

        g_gpu.present = 1;
        g_gpu.vendor = vendor_type(d->vendor_id);
        g_gpu.vendor_id = d->vendor_id;
        g_gpu.device_id = d->device_id;
        g_gpu.bus = d->bus;
        g_gpu.device = d->device;
        g_gpu.function = d->function;
        g_gpu.class_code = d->class_code;
        g_gpu.subclass = d->subclass;
        g_gpu.prog_if = d->prog_if;
        g_gpu.bar0 = pci_get_bar64(d, 0);
        g_gpu.vendor_name = vendor_name(d->vendor_id);
        g_gpu.device_name = device_name(d->vendor_id, d->device_id);

        /* Detection only for now: don't touch vendor registers. The UEFI GOP
         * framebuffer remains the safe display backend until a real vendor
         * command/VRAM driver is implemented. */
        return 1;
    }
    return 0;
}

int gpu_is_ready(void) { return g_gpu.present; }
const nexus_gpu_info_t *gpu_get_info(void) { return g_gpu.present ? &g_gpu : 0; }
const char *gpu_vendor_name(void) { return g_gpu.present ? g_gpu.vendor_name : "Unknown"; }
const char *gpu_device_name(void) { return g_gpu.present ? g_gpu.device_name : "Unknown GPU"; }
const char *gpu_name(void) { return gpu_device_name(); }
