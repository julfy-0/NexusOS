#include <stdint.h>
#include <stddef.h>
#include "uhci.h"
#include "pci.h"
#include "io.h"

#define PCI_CLASS_USB 0x0C
#define PCI_SUBCLASS_SERIAL 0x03
#define UHCI_PROGIF 0x00

#define USBCMD 0x00
#define USBSTS 0x02
#define USBINTR 0x04
#define FRNUM 0x06
#define FLBASEADD 0x08
#define SOFMOD 0x0C
#define PORTSC1 0x10
#define PORTSC2 0x12
#define CMD_RS 0x0001
#define CMD_HCRESET 0x0002
#define CMD_GRESET 0x0004
#define CMD_CONFIGURE 0x0040
#define STS_HCHALTED 0x0020
#define PORT_CCS 0x0001
#define PORT_CSC 0x0002
#define PORT_PE 0x0004
#define PORT_RESET 0x0200
#define PORT_RHSC 0x0008
#define UHCI_MAX_PORTS 8

static uint16_t g_io;
static int g_ready;
static uint8_t g_ports, g_connected;
static uint32_t g_frame_list[1024] __attribute__((aligned(4096)));

static uint16_t r16(uint16_t o) { return inw((uint16_t)(g_io + o)); }
static void w16(uint16_t o, uint16_t v) { outw((uint16_t)(g_io + o), v); }
static void delay(void) { for (volatile uint32_t i=0;i<50000;i++) __asm__ volatile("pause"); }

int uhci_init(void) {
    nexus_pci_device_t d;
    if (!pci_find_class(PCI_CLASS_USB, PCI_SUBCLASS_SERIAL, UHCI_PROGIF, &d)) return 0;
    if ((d.bar[4] & 1u) == 0 && (d.bar[0] & 1u) == 0) return 0;
    uint32_t raw = (d.bar[4] & 1u) ? d.bar[4] : d.bar[0];
    g_io = (uint16_t)(raw & 0xFFF0u);
    if (!g_io) return 0;
    pci_enable_device(&d, 0, 1);

    w16(USBCMD, CMD_GRESET);
    delay();
    w16(USBCMD, 0);
    for (int i=0;i<20;i++) { if (!(r16(USBCMD) & CMD_GRESET)) break; delay(); }

    for (size_t i=0;i<1024;i++) g_frame_list[i] = 1u;
    outl((uint16_t)(g_io + FLBASEADD), (uint32_t)(uintptr_t)g_frame_list);
    w16(FRNUM, 0);
    w16(SOFMOD, 64);
    w16(USBINTR, 0x0001 | 0x0004 | 0x0008);
    w16(USBCMD, CMD_CONFIGURE | CMD_RS);

    g_ports = 2;
    g_connected = 0;
    for (uint8_t p=0;p<g_ports;p++) {
        uint16_t ps = r16((uint16_t)(PORTSC1 + p*2));
        if (ps & PORT_CCS) g_connected++;
        /* Clear change bits without asserting reset. */
        if (ps & (PORT_CSC|PORT_RHSC)) w16((uint16_t)(PORTSC1+p*2), (uint16_t)(ps & (PORT_CSC|PORT_RHSC)));
    }
    g_ready = !(r16(USBSTS) & STS_HCHALTED);
    return g_ready;
}
int uhci_is_ready(void) { return g_ready; }
uint8_t uhci_port_count(void) { return g_ports; }
uint8_t uhci_connected_ports(void) { return g_connected; }
