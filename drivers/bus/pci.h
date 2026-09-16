#ifndef NEXUSOS_PCI_H
#define NEXUSOS_PCI_H

#include <stdint.h>

#define PCI_MAX_DEVICES 256

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


/* Полный физический адрес PCI Memory BAR (32/64-bit). 0 = invalid/I/O BAR. */
uint64_t pci_get_bar64(const nexus_pci_device_t *dev, int index);

/* Counts devices matching a PCI class/subclass/programming-interface tuple. */
int pci_count_class(uint8_t class_code, uint8_t subclass, uint8_t prog_if, int exact_prog_if);

/* Включает PCI Memory Space и/или Bus Mastering. */
void pci_enable_device(const nexus_pci_device_t *dev, int memory_space, int bus_master);

#endif
