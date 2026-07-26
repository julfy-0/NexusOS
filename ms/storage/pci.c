/* NexusOS: перечисление устройств на шине PCI через config mechanism #1
 * (порты 0xCF8/0xCFC) — так работает на всех x86 машинах с 1990-х,
 * MMCONFIG/ECAM для этого не нужен. */
#include "pci.h"
#include "io.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

static nexus_pci_device_t g_devices[PCI_MAX_DEVICES];
static int g_device_count = 0;

static uint32_t pci_config_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t address = (1u << 31) |
                        ((uint32_t)bus << 16) |
                        ((uint32_t)dev << 11) |
                        ((uint32_t)func << 8) |
                        (offset & 0xFC);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static void scan_function(uint8_t bus, uint8_t dev, uint8_t func) {
    uint32_t id = pci_config_read32(bus, dev, func, 0x00);
    uint16_t vendor_id = id & 0xFFFF;
    if (vendor_id == 0xFFFF) {
        return; /* нет устройства */
    }

    if (g_device_count >= PCI_MAX_DEVICES) {
        return;
    }

    uint16_t device_id = (id >> 16) & 0xFFFF;
    uint32_t class_reg = pci_config_read32(bus, dev, func, 0x08);

    nexus_pci_device_t *d = &g_devices[g_device_count++];
    d->bus = bus;
    d->device = dev;
    d->function = func;
    d->vendor_id = vendor_id;
    d->device_id = device_id;
    d->revision   = (class_reg >> 0) & 0xFF;
    d->prog_if    = (class_reg >> 8) & 0xFF;
    d->subclass   = (class_reg >> 16) & 0xFF;
    d->class_code = (class_reg >> 24) & 0xFF;

    for (int i = 0; i < 6; i++) {
        d->bar[i] = pci_config_read32(bus, dev, func, (uint8_t)(0x10 + i * 4));
    }
}

void pci_scan(void) {
    g_device_count = 0;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint32_t id0 = pci_config_read32((uint8_t)bus, dev, 0, 0x00);
            if ((id0 & 0xFFFF) == 0xFFFF) {
                continue; /* устройства нет вообще */
            }

            scan_function((uint8_t)bus, dev, 0);

            uint32_t header = pci_config_read32((uint8_t)bus, dev, 0, 0x0C);
            int multi_function = (header >> 16) & 0x80;
            if (multi_function) {
                for (uint8_t func = 1; func < 8; func++) {
                    scan_function((uint8_t)bus, dev, func);
                }
            }

            if (g_device_count >= PCI_MAX_DEVICES) {
                return;
            }
        }
    }
}

int pci_get_device_count(void) {
    return g_device_count;
}

const nexus_pci_device_t *pci_get_device(int index) {
    if (index < 0 || index >= g_device_count) return 0;
    return &g_devices[index];
}

int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if, nexus_pci_device_t *out) {
    for (int i = 0; i < g_device_count; i++) {
        if (g_devices[i].class_code == class_code &&
            g_devices[i].subclass == subclass &&
            g_devices[i].prog_if == prog_if) {
            if (out) *out = g_devices[i];
            return 1;
        }
    }
    return 0;
}
