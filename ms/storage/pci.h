#ifndef NEXUSOS_PCI_H
#define NEXUSOS_PCI_H

#include <stdint.h>

#define PCI_MAX_DEVICES 64

typedef struct {
    uint8_t  bus, device, function;
    uint16_t vendor_id, device_id;
    uint8_t  class_code, subclass, prog_if, revision;
    uint32_t bar[6]; /* сырые значения BAR0..BAR5, без разбора типа/размера */
} nexus_pci_device_t;

/* Сканирует шины 0-255 (brute force через config mechanism #1, порты
 * 0xCF8/0xCFC) и запоминает найденные устройства. Вызывать один раз. */
void pci_scan(void);

int pci_get_device_count(void);
const nexus_pci_device_t *pci_get_device(int index);

/* Ищет первое устройство с заданным классом/подклассом/интерфейсом.
 * Возвращает 1 и заполняет *out, если нашлось, иначе 0. */
int pci_find_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if,
                    nexus_pci_device_t *out);

#endif
