#ifndef NEXUSOS_USB_H
#define NEXUSOS_USB_H
#include <stdint.h>

typedef enum {
    USB_HOST_NONE = 0,
    USB_HOST_UHCI,
    USB_HOST_OHCI,
    USB_HOST_EHCI,
    USB_HOST_XHCI
} usb_host_type_t;

int usb_init(void);
usb_host_type_t usb_host_type(void);
uint8_t usb_host_count(void);
uint8_t usb_port_count(void);
uint8_t usb_connected_ports(void);
const char *usb_host_name(void);
/* Diagnostics for real-hardware bring-up. */
uint8_t usb_pci_controller_count(void);
const char *usb_last_error(void);
int usb_xhci_controller_count(void);
#endif
