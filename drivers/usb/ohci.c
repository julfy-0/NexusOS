#include <stdint.h>
#include <stddef.h>
#include "ohci.h"
#include "pci.h"
#include "paging.h"

#define CLASS_USB 0x0C
#define SUBCLASS_USB 0x03
#define PROGIF_OHCI 0x10
#define OHCI_MAX_PORTS 15

#define HcControl 0x04
#define HcCommandStatus 0x08
#define HcInterruptStatus 0x0C
#define HcInterruptEnable 0x10
#define HcHCCA 0x18
#define HcControlHeadED 0x20
#define HcBulkHeadED 0x28
#define HcControlCurrentED 0x2C
#define HcBulkCurrentED 0x30
#define HcDoneHead 0x34
#define HcFmInterval 0x38
#define HcPeriodicStart 0x40
#define HcRhDescriptorA 0x48
#define HcRhDescriptorB 0x4C
#define HcRhStatus 0x50
#define HcRhPortStatus 0x54
#define HC_RESET 0x00000001
#define HC_CONTROL_FS_MASK 0xC0000000u
#define HC_CONTROL_PLE 0x00000004u
#define HC_CONTROL_IE 0x00000008u
#define HC_CONTROL_CLE 0x00000010u
#define HC_CONTROL_BLE 0x00000020u
#define HC_CONTROL_IR 0x00000040u
#define CMD_HCR 0x00000001u
#define CMD_OCR 0x00000008u
#define PORT_CCS 0x00000001u
#define PORT_PES 0x00000002u
#define PORT_PSS 0x00000008u
#define PORT_CSC 0x00010000u
#define PORT_PESC 0x00020000u
#define PORT_PRSC 0x00100000u
#define RH_NDP_MASK 0x000000FFu

static volatile uint8_t *g_mmio;
static int g_ready;
static uint8_t g_ports, g_connected;
static uint8_t g_hcca[256] __attribute__((aligned(256)));
static uint32_t g_periodic[32] __attribute__((aligned(256)));

static uint32_t rd(uint32_t o) { return *(volatile uint32_t *)(g_mmio+o); }
static void wr(uint32_t o,uint32_t v) { *(volatile uint32_t *)(g_mmio+o)=v; }
static void delay(void) { for (volatile uint32_t i=0;i<100000;i++) __asm__ volatile("pause"); }

int ohci_init(void) {
    nexus_pci_device_t d;
    if (!pci_find_class(CLASS_USB,SUBCLASS_USB,PROGIF_OHCI,&d)) return 0;
    uint64_t bar = pci_get_bar64(&d,0);
    if (!bar) return 0;
    pci_enable_device(&d,1,1);
    if(bar >= (512ULL*1024ULL*1024ULL*1024ULL)) return 0;
    paging_map_region(bar, bar + 0x10000ULL);
    g_mmio=(volatile uint8_t *)(uintptr_t)bar;

    uint32_t control=rd(HcControl);
    if (control & HC_CONTROL_IR) {
        /* BIOS/SMM ownership: request ownership change where available. */
        wr(HcCommandStatus, CMD_OCR);
        delay();
    }
    wr(HcCommandStatus, CMD_HCR);
    for (int i=0;i<100;i++) { if (!(rd(HcCommandStatus)&CMD_HCR)) break; delay(); }

    for (size_t i=0;i<32;i++) g_periodic[i]=0;
    wr(HcHCCA,(uint32_t)(uintptr_t)g_hcca);
    wr(HcControlHeadED,0);
    wr(HcBulkHeadED,0);
    uint32_t fi=rd(HcFmInterval);
    if (!fi) fi=0x2EDF;
    wr(HcFmInterval,fi);
    wr(HcPeriodicStart,(fi & 0x3FFFu)*9u/10u);
    wr(HcInterruptStatus,0xFFFFFFFFu);
    wr(HcInterruptEnable,0x8000007Fu);
    wr(HcControl,(rd(HcControl)&~HC_CONTROL_FS_MASK)|0x00000080u|HC_CONTROL_PLE|HC_CONTROL_CLE|HC_CONTROL_BLE);

    uint32_t rh=rd(HcRhDescriptorA);
    g_ports=(uint8_t)(rh&RH_NDP_MASK);
    if (!g_ports || g_ports>OHCI_MAX_PORTS) g_ports=OHCI_MAX_PORTS;
    g_connected=0;
    for(uint8_t p=0;p<g_ports;p++) {
        uint32_t ps=rd(HcRhPortStatus+p*4u);
        if(ps&PORT_CCS) g_connected++;
        uint32_t changes=ps&(PORT_CSC|PORT_PESC|PORT_PRSC);
        if(changes) wr(HcRhPortStatus+p*4u,changes);
    }
    g_ready=1;
    return 1;
}
int ohci_is_ready(void){return g_ready;}
uint8_t ohci_port_count(void){return g_ports;}
uint8_t ohci_connected_ports(void){return g_connected;}
