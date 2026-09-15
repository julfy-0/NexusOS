#include "network.h"
#include "pci.h"
#include "console.h"
#include "e1000.h"

static nexus_network_info_t g_net;

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

void nexus_network_init(void) {
    g_net.available = 0;
    g_net.tx_packets = g_net.rx_packets = g_net.dropped_packets = 0;
    for (int i = 0; i < pci_get_device_count(); ++i) {
        const nexus_pci_device_t *d = pci_get_device(i);
        if (d && d->class_code == 0x02) {
            g_net.available = 1;
            g_net.vendor_id = d->vendor_id;
            g_net.device_id = d->device_id;
            g_net.bus = d->bus; g_net.device = d->device; g_net.function = d->function;
            pci_enable_device(d, 1, 1);
            if (e1000_init(d)) {
                const nexus_e1000_info_t *e = e1000_info();
                g_net.driver_ready = 1;
                for (int j = 0; j < 6; ++j) g_net.mac[j] = e->mac[j];
                console_print("[NET] Intel E1000 driver bound\n");
            } else {
                console_print("[NET] PCI network controller detected, no bound NIC driver\n");
            }
            return;
        }
    }
    console_print("[NET] No supported PCI network controller driver\n");
}

const nexus_network_info_t *nexus_network_info(void) {
    const nexus_e1000_info_t *e = e1000_info();
    if (e && e->present) { g_net.tx_packets=e->tx_packets; g_net.rx_packets=e->rx_packets; }
    return &g_net;
}

int nexus_network_send(const void *frame, uint32_t len) { return e1000_send(frame,len); }
int nexus_network_poll(uint8_t *frame, uint32_t capacity, uint32_t *len) { return e1000_poll(frame,capacity,len); }

int nexus_network_parse_ethernet(const uint8_t *frame, uint32_t len, uint16_t *ethertype) {
    if (!frame || len < 14) { g_net.dropped_packets++; return 0; }
    if (ethertype) *ethertype = be16(frame + 12);
    return 1;
}

uint16_t nexus_network_checksum(const void *data, uint32_t len) {
    const uint8_t *p = (const uint8_t *)data; uint32_t sum = 0;
    while (len > 1) { sum += be16(p); p += 2; len -= 2; }
    if (len) sum += (uint16_t)(p[0] << 8);
    while (sum >> 16) sum = (sum & 0xffffU) + (sum >> 16);
    return (uint16_t)~sum;
}

int nexus_network_parse_ipv4(const uint8_t *packet, uint32_t len, uint8_t *protocol) {
    if (!packet || len < 20 || (packet[0] >> 4) != 4) { g_net.dropped_packets++; return 0; }
    uint32_t ihl = (uint32_t)(packet[0] & 0x0f) * 4;
    if (ihl < 20 || len < ihl || be16(packet + 2) < ihl) { g_net.dropped_packets++; return 0; }
    if (protocol) *protocol = packet[9];
    return 1;
}
