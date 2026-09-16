#include <stdint.h>
#include "usb.h"
#include "pci.h"
#include "uhci.h"
#include "ohci.h"
#include "ehci.h"
#include "xhci.h"

static usb_host_type_t g_type=USB_HOST_NONE;
static uint8_t g_count,g_connected;
static uint8_t g_pci_controllers;
static const char *g_last_error = "no USB controller";

int usb_init(void){
    g_type=USB_HOST_NONE; g_count=0; g_connected=0; g_pci_controllers=0;
    g_last_error="no USB controller";

    pci_scan();
    for(int i=0;i<pci_get_device_count();i++){
        const nexus_pci_device_t *d=pci_get_device(i);
        if(d && d->class_code==0x0C && d->subclass==0x03) g_pci_controllers++;
    }
    if(!g_pci_controllers){
        g_last_error="PCI scan found no USB host controller";
        return 0;
    }

    if(xhci_init()){g_type=USB_HOST_XHCI;g_count=1;g_connected=xhci_connected_ports();g_last_error="none";return 1;}
    if(ehci_init()){g_type=USB_HOST_EHCI;g_count=1;g_connected=ehci_connected_ports();g_last_error="none";return 1;}
    if(ohci_init()){g_type=USB_HOST_OHCI;g_count=1;g_connected=ohci_connected_ports();g_last_error="none";return 1;}
    if(uhci_init()){g_type=USB_HOST_UHCI;g_count=1;g_connected=uhci_connected_ports();g_last_error="none";return 1;}

    g_last_error="USB PCI controller detected, but initialization failed";
    return 0;
}

usb_host_type_t usb_host_type(void){return g_type;}
uint8_t usb_host_count(void){return g_count;}
uint8_t usb_port_count(void){
    switch(g_type){case USB_HOST_UHCI:return uhci_port_count();case USB_HOST_OHCI:return ohci_port_count();case USB_HOST_EHCI:return ehci_port_count();case USB_HOST_XHCI:return xhci_port_count();default:return 0;}
}
uint8_t usb_connected_ports(void){return g_connected;}
uint8_t usb_pci_controller_count(void){return g_pci_controllers;}
const char *usb_last_error(void){return g_last_error;}
int usb_xhci_controller_count(void){return xhci_controller_count();}
const char *usb_host_name(void){
    switch(g_type){case USB_HOST_UHCI:return "UHCI / USB 1.x";case USB_HOST_OHCI:return "OHCI / USB 1.x";case USB_HOST_EHCI:return "EHCI / USB 2.0";case USB_HOST_XHCI:return "xHCI / USB 3.x";default:return "none";}
}
