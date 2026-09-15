#ifndef NEXUSOS_NETWORK_H
#define NEXUSOS_NETWORK_H

#include <stdint.h>

typedef struct {
    int available;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus, device, function;
    uint32_t tx_packets, rx_packets, dropped_packets;
    int driver_ready;
    uint8_t mac[6];
} nexus_network_info_t;

void nexus_network_init(void);
const nexus_network_info_t *nexus_network_info(void);
/* Foundation packet validators used by future NIC drivers. */
int nexus_network_parse_ethernet(const uint8_t *frame, uint32_t len, uint16_t *ethertype);
int nexus_network_parse_ipv4(const uint8_t *packet, uint32_t len, uint8_t *protocol);
uint16_t nexus_network_checksum(const void *data, uint32_t len);
int nexus_network_send(const void *frame, uint32_t len);
int nexus_network_poll(uint8_t *frame, uint32_t capacity, uint32_t *len);

#endif
