#include <stdint.h>
#include <stddef.h>
#include "ehci.h"
#include "pci.h"
#include "paging.h"

#define CLASS_USB 0x0C
#define SUBCLASS_USB 0x03
#define PROGIF_EHCI 0x20
#define CMD 0x00
#define STATUS 0x04
#define INTR 0x08
#define FRINDEX 0x0C
#define CTRLDSSEG 0x10
#define PERIODICLIST 0x14
#define ASYNCLIST 0x18
#define CONFIGFLAG 0x40
#define PORTSC 0x44
#define CMD_RUN 0x00000001u
#define CMD_RESET 0x00000002u
#define STS_HALTED 0x00001000u
#define STS_PCD 0x00000004u
#define PORT_CCS 0x00000001u
#define PORT_PED 0x00000004u
#define PORT_PR 0x00000010u
#define PORT_PP 0x00001000u
#define PORT_CSC 0x00000002u
#define PORT_PEC 0x00000008u
#define PORT_OCC 0x00000020u
#define MAX_PORTS 15

static volatile uint8_t *g_op;
static int g_ready;
static uint8_t g_ports,g_connected;
static uint32_t g_periodic[1024] __attribute__((aligned(4096)));

typedef struct { uint32_t horiz, epchar, epcap, current; uint32_t next, alt, token; uint32_t buf[5]; } __attribute__((aligned(32))) ehci_qh_t;
static ehci_qh_t g_async_qh __attribute__((aligned(32)));
static uint32_t rd(uint32_t o){return *(volatile uint32_t *)(g_op+o);}
static void wr(uint32_t o,uint32_t v){*(volatile uint32_t *)(g_op+o)=v;}
static void delay(void){for(volatile uint32_t i=0;i<100000;i++) __asm__ volatile("pause");}

int ehci_init(void){
    nexus_pci_device_t d;
    if(!pci_find_class(CLASS_USB,SUBCLASS_USB,PROGIF_EHCI,&d)) return 0;
    uint64_t bar=pci_get_bar64(&d,0); if(!bar)return 0;
    pci_enable_device(&d,1,1);
    if(bar >= (512ULL*1024ULL*1024ULL*1024ULL)) return 0;
    paging_map_region(bar, bar + 0x20000ULL);
    volatile uint8_t *cap=(volatile uint8_t *)(uintptr_t)bar;
    uint8_t caplen=cap[0];
    g_op=cap+caplen;
    wr(CMD,rd(CMD)&~CMD_RUN);
    for(int i=0;i<100;i++){if(rd(STATUS)&STS_HALTED)break;delay();}
    wr(CMD,rd(CMD)|CMD_RESET);
    for(int i=0;i<100;i++){if(!(rd(CMD)&CMD_RESET))break;delay();}
    for(size_t i=0;i<1024;i++)g_periodic[i]=1u;
    g_async_qh.horiz=(uint32_t)(uintptr_t)&g_async_qh|1u;
    g_async_qh.epchar=0;
    g_async_qh.epcap=0;
    g_async_qh.current=0;
    g_async_qh.next=1u;
    g_async_qh.alt=1u;
    g_async_qh.token=0x40u;
    for(int i=0;i<5;i++)g_async_qh.buf[i]=0;
    wr(CTRLDSSEG,0);
    wr(PERIODICLIST,(uint32_t)(uintptr_t)g_periodic);
    wr(ASYNCLIST,(uint32_t)(uintptr_t)&g_async_qh);
    wr(INTR,0x3Fu);
    wr(CONFIGFLAG,1);
    uint32_t hcc=(uint32_t)cap[8]|((uint32_t)cap[9]<<8)|((uint32_t)cap[10]<<16)|((uint32_t)cap[11]<<24);
    (void)hcc;
    wr(CMD,rd(CMD)|CMD_RUN);
    g_ports=0; g_connected=0;
    /* EHCI port count is encoded in HCSParams bits 0..3 at capability offset 4. */
    uint32_t hcs=(uint32_t)cap[4]|((uint32_t)cap[5]<<8)|((uint32_t)cap[6]<<16)|((uint32_t)cap[7]<<24);
    g_ports=(uint8_t)(hcs&0x0Fu); if(g_ports>MAX_PORTS)g_ports=MAX_PORTS;
    for(uint8_t p=0;p<g_ports;p++){
        uint32_t ps=rd(PORTSC+p*4u);
        if(ps&PORT_CCS)g_connected++;
        uint32_t ch=ps&(PORT_CSC|PORT_PEC|PORT_OCC);
        if(ch)wr(PORTSC+p*4u,ch| (ps&PORT_PP));
    }
    g_ready=!(rd(STATUS)&STS_HALTED);
    return g_ready;
}
int ehci_is_ready(void){return g_ready;}
uint8_t ehci_port_count(void){return g_ports;}
uint8_t ehci_connected_ports(void){return g_connected;}
