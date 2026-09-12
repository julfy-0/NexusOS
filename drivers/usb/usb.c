#include <stdint.h>
#include "usb.h"
#include "pci.h"
#include "uhci.h"
#include "ohci.h"
#include "ehci.h"
#include "xhci.h"

static usb_host_type_t g_type=USB_HOST_NONE;
static uint8_t g_count,g_connected;

int usb_init(void){
    /* One PCI function normally represents one host controller. Prefer the
       newest controller first; legacy controllers are still supported. */
    if(xhci_init()){g_type=USB_HOST_XHCI;g_count=1;g_connected=xhci_connected_ports();return 1;}
    if(ehci_init()){g_type=USB_HOST_EHCI;g_count=1;g_connected=ehci_connected_ports();return 1;}
    if(ohci_init()){g_type=USB_HOST_OHCI;g_count=1;g_connected=ohci_connected_ports();return 1;}
    if(uhci_init()){g_type=USB_HOST_UHCI;g_count=1;g_connected=uhci_connected_ports();return 1;}
    return 0;
}
usb_host_type_t usb_host_type(void){return g_type;}
uint8_t usb_host_count(void){return g_count;}
uint8_t usb_port_count(void){
    switch(g_type){case USB_HOST_UHCI:return uhci_port_count();case USB_HOST_OHCI:return ohci_port_count();case USB_HOST_EHCI:return ehci_port_count();case USB_HOST_XHCI:return xhci_port_count();default:return 0;}
}
uint8_t usb_connected_ports(void){return g_connected;}
const char *usb_host_name(void){
    switch(g_type){case USB_HOST_UHCI:return "UHCI / USB 1.x";case USB_HOST_OHCI:return "OHCI / USB 1.x";case USB_HOST_EHCI:return "EHCI / USB 2.0";case USB_HOST_XHCI:return "xHCI / USB 3.x";default:return "none";}
}
