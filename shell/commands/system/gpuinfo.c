#include "gpuinfo.h"
#include "console.h"
#include "gpu.h"

static void print_hex16(uint16_t v) {
    const char *d = "0123456789ABCDEF";
    char s[5] = {d[(v >> 12) & 15], d[(v >> 8) & 15], d[(v >> 4) & 15], d[v & 15], 0};
    console_print(s);
}

void gpuinfo_run(void) {
    const nexus_gpu_info_t *g = gpu_get_info();
    if (!g) {
        console_print("gpuinfo: no PCI display controller detected\n");
        return;
    }

    console_print("GPU information\n");
    console_print("  Vendor:       "); console_print(g->vendor_name); console_print("\n");
    console_print("  Device:       "); console_print(g->device_name); console_print("\n");
    console_print("  PCI:          ");
    console_print_dec(g->bus); console_print(":");
    console_print_dec(g->device); console_print(".");
    console_print_dec(g->function); console_print("\n");
    console_print("  Vendor ID:    0x"); print_hex16(g->vendor_id); console_print("\n");
    console_print("  Device ID:    0x"); print_hex16(g->device_id); console_print("\n");
    console_print("  Class:        0x"); print_hex16((uint16_t)((g->class_code << 8) | g->subclass));
    console_print(" / 0x"); print_hex16(g->prog_if); console_print("\n");
    console_print("  BAR0:         0x"); console_print_hex(g->bar0); console_print("\n");
    console_print("  Detection:    READY\n");
}
