#include "e1000.h"
#include "console.h"
#include "pmm.h"
#include "memory.h"

#define E1000_VENDOR_INTEL 0x8086
#define E1000_REG_CTRL 0x0000
#define E1000_REG_STATUS 0x0008
#define E1000_REG_RCTL 0x0100
#define E1000_REG_TCTL 0x0400
#define E1000_REG_TIPG 0x0410
#define E1000_REG_RDBAL 0x2800
#define E1000_REG_RDBAH 0x2804
#define E1000_REG_RDLEN 0x2808
#define E1000_REG_RDH 0x2810
#define E1000_REG_RDT 0x2818
#define E1000_REG_TDBAL 0x3800
#define E1000_REG_TDBAH 0x3804
#define E1000_REG_TDLEN 0x3808
#define E1000_REG_TDH 0x3810
#define E1000_REG_TDT 0x3818
#define E1000_REG_RAL 0x5400
#define E1000_REG_RAH 0x5404
#define E1000_RCTL_EN 0x00000002
#define E1000_RCTL_BAM 0x00008000
#define E1000_RCTL_SECRC 0x04000000
#define E1000_TCTL_EN 0x00000002
#define E1000_TCTL_PSP 0x00000008
#define E1000_TX_CMD_EOP 0x01
#define E1000_TX_CMD_RS 0x08
#define E1000_TX_STATUS_DD 0x01
#define E1000_RX_STATUS_DD 0x01
#define E1000_RX_STATUS_EOP 0x02
#define E1000_RING_COUNT 16
#define E1000_BUFFER_SIZE 2048

typedef struct __attribute__((packed)) { uint64_t addr; uint16_t length; uint8_t cso; uint8_t cmd; uint8_t status; uint8_t css; uint16_t special; } tx_desc_t;
typedef struct __attribute__((packed)) { uint64_t addr; uint16_t length; uint16_t checksum; uint8_t status; uint8_t errors; uint16_t special; } rx_desc_t;

static nexus_e1000_info_t g_e1000;
static volatile uint8_t *g_regs;
static tx_desc_t *g_tx;
static rx_desc_t *g_rx;
static uint64_t g_tx_phys[E1000_RING_COUNT], g_rx_phys[E1000_RING_COUNT];
static uint16_t g_tx_tail, g_rx_next;
static uint32_t reg_read(uint32_t off) { return *(volatile uint32_t *)(g_regs + off); }
static void reg_write(uint32_t off, uint32_t v) { *(volatile uint32_t *)(g_regs + off) = v; }
static int supported(uint16_t id) { switch (id) { case 0x100E: case 0x100F: case 0x10D3: case 0x153A: return 1; default: return 0; } }

static int setup_rings(void) {
    uint64_t txring = pmm_alloc_page(), rxring = pmm_alloc_page();
    if (!txring || !rxring) return 0;
    g_tx = (tx_desc_t *)(uintptr_t)txring; g_rx = (rx_desc_t *)(uintptr_t)rxring;
    memset(g_tx, 0, 4096); memset(g_rx, 0, 4096);
    for (uint32_t i=0;i<E1000_RING_COUNT;i++) {
        g_tx_phys[i]=pmm_alloc_page(); g_rx_phys[i]=pmm_alloc_page();
        if (!g_tx_phys[i] || !g_rx_phys[i]) return 0;
        memset((void *)(uintptr_t)g_tx_phys[i],0,4096);
        memset((void *)(uintptr_t)g_rx_phys[i],0,4096);
        g_rx[i].addr=g_rx_phys[i]; g_tx[i].status=E1000_TX_STATUS_DD;
    }
    reg_write(E1000_REG_TDBAL,(uint32_t)txring); reg_write(E1000_REG_TDBAH,(uint32_t)(txring>>32));
    reg_write(E1000_REG_TDLEN,E1000_RING_COUNT*sizeof(tx_desc_t)); reg_write(E1000_REG_TDH,0); reg_write(E1000_REG_TDT,0);
    reg_write(E1000_REG_TIPG,0x0060200A); reg_write(E1000_REG_TCTL,E1000_TCTL_EN|E1000_TCTL_PSP|(0x10<<4)|(0x40<<12));
    reg_write(E1000_REG_RDBAL,(uint32_t)rxring); reg_write(E1000_REG_RDBAH,(uint32_t)(rxring>>32));
    reg_write(E1000_REG_RDLEN,E1000_RING_COUNT*sizeof(rx_desc_t)); reg_write(E1000_REG_RDH,0); reg_write(E1000_REG_RDT,E1000_RING_COUNT-1);
    reg_write(E1000_REG_RCTL,E1000_RCTL_EN|E1000_RCTL_BAM|E1000_RCTL_SECRC);
    return 1;
}
int e1000_init(const nexus_pci_device_t *dev) {
    g_e1000.present=0;
    if (!dev || dev->vendor_id!=E1000_VENDOR_INTEL || !supported(dev->device_id)) return 0;
    uint64_t bar=pci_get_bar64(dev,0); if(!bar) return 0;
    pci_enable_device(dev,1,1); g_regs=(volatile uint8_t *)(uintptr_t)bar;
    uint32_t ral=reg_read(E1000_REG_RAL), rah=reg_read(E1000_REG_RAH);
    g_e1000.present=1; g_e1000.vendor_id=dev->vendor_id; g_e1000.device_id=dev->device_id; g_e1000.mmio_base=bar;
    for (int i=0;i<4;i++) g_e1000.mac[i]=(ral>>(i*8))&0xff;
    g_e1000.mac[4]=rah&0xff;
    g_e1000.mac[5]=(rah>>8)&0xff;
    if(!setup_rings()) { g_e1000.present=0; return 0; }
    console_print("[NET] Intel E1000 RX/TX DMA rings initialized\n"); return 1;
}
const nexus_e1000_info_t *e1000_info(void){return &g_e1000;}
int e1000_send(const void *frame,uint32_t len){
    if(!g_e1000.present||!frame||len==0||len>E1000_BUFFER_SIZE) return 0;
    tx_desc_t *d=&g_tx[g_tx_tail]; if(!(d->status&E1000_TX_STATUS_DD)) return 0;
    memcpy((void *)(uintptr_t)g_tx_phys[g_tx_tail],frame,len); d->addr=g_tx_phys[g_tx_tail]; d->length=len; d->cmd=E1000_TX_CMD_EOP|E1000_TX_CMD_RS; d->status=0;
    g_tx_tail=(g_tx_tail+1)%E1000_RING_COUNT; reg_write(E1000_REG_TDT,g_tx_tail); g_e1000.tx_packets++; return 1;
}
int e1000_poll(uint8_t *frame,uint32_t capacity,uint32_t *len){
    if (len) *len=0;
    if (!g_e1000.present || !frame) return 0;
    rx_desc_t *d=&g_rx[g_rx_next];
    if (!(d->status&E1000_RX_STATUS_DD)) return 0;
    uint32_t n=d->length;
    int ok=(d->status&E1000_RX_STATUS_EOP)&&n<=capacity&&n<=E1000_BUFFER_SIZE;
    if(ok){memcpy(frame,(void *)(uintptr_t)g_rx_phys[g_rx_next],n); if(len)*len=n; g_e1000.rx_packets++;}
    d->status=0; d->length=0; reg_write(E1000_REG_RDT,g_rx_next); g_rx_next=(g_rx_next+1)%E1000_RING_COUNT; return ok;
}
