#ifndef NEXUSOS_E1000_H
#define NEXUSOS_E1000_H
#include <stdint.h>
#include "pci.h"

typedef struct {
    int present;
    uint16_t vendor_id, device_id;
    uint64_t mmio_base;
    uint8_t mac[6];
    uint32_t tx_packets, rx_packets;
} nexus_e1000_info_t;

int e1000_init(const nexus_pci_device_t *dev);
const nexus_e1000_info_t *e1000_info(void);
/* Normal-context polling API. No packet processing is done in IRQ context. */
int e1000_send(const void *frame, uint32_t len);
int e1000_poll(uint8_t *frame, uint32_t capacity, uint32_t *len);
#endif
