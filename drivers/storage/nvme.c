/* NexusOS NVMe controller driver.
 *
 * This is a deliberately small polling implementation of the NVMe register,
 * admin-queue and I/O-queue programming model. DMA addresses are identity
 * mapped physical addresses in the current NexusOS kernel, so the static
 * queue/data objects below can be handed directly to the controller.
 */
#include "nvme.h"
#include "pci.h"

extern void *memset(void *dest, int value, unsigned long n);

#define NVME_CLASS       0x01
#define NVME_SUBCLASS    0x08
#define NVME_PROGIF      0x02

#define NVME_REG_CAP     0x0000
#define NVME_REG_VS      0x0008
#define NVME_REG_CC      0x0014
#define NVME_REG_CSTS    0x001C
#define NVME_REG_AQA     0x0024
#define NVME_REG_ASQ     0x0028
#define NVME_REG_ACQ     0x0030

#define NVME_CC_EN       (1u << 0)
#define NVME_CC_CSS_NVM  (0u << 4)
#define NVME_CC_MPS_4K   (0u << 7)
#define NVME_CC_IOSQES   (6u << 16)
#define NVME_CC_IOCQES   (4u << 20)

#define NVME_CSTS_RDY    (1u << 0)
#define NVME_CSTS_CFS    (1u << 1)

#define NVME_ADMIN_CREATE_IO_CQ 0x05
#define NVME_ADMIN_CREATE_IO_SQ 0x01
#define NVME_ADMIN_IDENTIFY     0x06
#define NVME_IO_READ            0x02

#define NVME_QDEPTH 16
#define NVME_PAGE_SIZE 4096u
#define NVME_MAX_SECTORS 128u
#define NVME_ADMIN_TIMEOUT 10000000u
#define NVME_IO_TIMEOUT    10000000u

/* Submission/completion entries are 64 bytes each. */
typedef struct {
    uint32_t d[16];
} nvme_cmd_t;

typedef struct {
    uint32_t dw0;
    uint32_t dw1;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
} nvme_cqe_t;

static volatile uint8_t *g_regs;
static uint64_t g_cap;
static uint32_t g_db_stride;
static uint16_t g_cid;
static uint16_t g_asq_tail;
static uint16_t g_acq_head;
static uint8_t g_acq_phase;
static uint16_t g_iosq_tail;
static uint16_t g_iocq_head;
static uint8_t g_iocq_phase;
static uint32_t g_sector_size;
static uint64_t g_ns_sectors;
static uint16_t g_vendor_id;
static uint16_t g_device_id;
static uint8_t g_vs_major;
static uint8_t g_vs_minor;
static int g_ready;

static nvme_cmd_t g_admin_sq[NVME_QDEPTH] __attribute__((aligned(4096)));
static nvme_cqe_t g_admin_cq[NVME_QDEPTH] __attribute__((aligned(4096)));
static nvme_cmd_t g_io_sq[NVME_QDEPTH] __attribute__((aligned(4096)));
static nvme_cqe_t g_io_cq[NVME_QDEPTH] __attribute__((aligned(4096)));
static uint8_t g_identify[4096] __attribute__((aligned(4096)));
static uint64_t g_prp_list[512] __attribute__((aligned(4096)));

static inline uint32_t mmio32(uint32_t off) {
    return *(volatile uint32_t *)(g_regs + off);
}

static inline void mmio32w(uint32_t off, uint32_t value) {
    *(volatile uint32_t *)(g_regs + off) = value;
}

static inline uint64_t mmio64(uint32_t off) {
    uint32_t lo = mmio32(off);
    uint32_t hi = mmio32(off + 4);
    return ((uint64_t)hi << 32) | lo;
}

static inline uint64_t phys(const void *p) {
    return (uint64_t)(uintptr_t)p;
}

static int wait_reg(uint32_t off, uint32_t mask, uint32_t wanted, uint32_t limit) {
    for (uint32_t i = 0; i < limit; i++) {
        if ((mmio32(off) & mask) == wanted) return 1;
    }
    return 0;
}

static void ring_sq(uint16_t qid, uint16_t tail) {
    uint32_t off = 0x1000u + (uint32_t)(2u * qid) * g_db_stride;
    mmio32w(off, tail);
}

static void ring_cq(uint16_t qid, uint16_t head) {
    uint32_t off = 0x1000u + (uint32_t)(2u * qid + 1u) * g_db_stride;
    mmio32w(off, head);
}

static int completion_ok(const nvme_cqe_t *cqe) {
    /* Status field: phase bit is bit 0, status code starts at bit 1. */
    return ((cqe->status >> 1) & 0x7FFu) == 0;
}

static int admin_submit(nvme_cmd_t *cmd) {
    uint16_t cid = g_cid++;
    cmd->d[0] &= 0x0000FFFFu;
    cmd->d[0] |= (uint32_t)cid << 16;

    g_admin_sq[g_asq_tail] = *cmd;
    g_asq_tail = (uint16_t)((g_asq_tail + 1) % NVME_QDEPTH);
    ring_sq(0, g_asq_tail);

    for (uint32_t i = 0; i < NVME_ADMIN_TIMEOUT; i++) {
        nvme_cqe_t *cqe = &g_admin_cq[g_acq_head];
        if ((cqe->status & 1u) == g_acq_phase) {
            int ok = completion_ok(cqe);
            g_acq_head = (uint16_t)((g_acq_head + 1) % NVME_QDEPTH);
            if (g_acq_head == 0) g_acq_phase ^= 1u;
            ring_cq(0, g_acq_head);
            return ok;
        }
    }
    return 0;
}

static int io_submit_read(uint64_t lba, uint32_t count, void *buf) {
    nvme_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));

    uint64_t first = phys(buf);
    uint64_t last = first + (uint64_t)count * 512u - 1u;
    uint64_t first_page = first & ~(uint64_t)(NVME_PAGE_SIZE - 1u);
    uint64_t last_page = last & ~(uint64_t)(NVME_PAGE_SIZE - 1u);
    uint32_t pages = (uint32_t)((last_page - first_page) / NVME_PAGE_SIZE) + 1u;

    if (pages == 0 || pages > 513u) return 0;

    cmd.d[0] = NVME_IO_READ;
    cmd.d[1] = 0; /* NSID is filled below. */
    cmd.d[10] = (uint32_t)lba;
    cmd.d[11] = (uint32_t)(lba >> 32);
    cmd.d[12] = (count - 1u) & 0xFFFFu;

    cmd.d[6] = (uint32_t)first;
    cmd.d[7] = (uint32_t)(first >> 32);

    if (pages == 1u) {
        cmd.d[8] = 0;
        cmd.d[9] = 0;
    } else if (pages == 2u) {
        uint64_t second = first_page + NVME_PAGE_SIZE;
        cmd.d[8] = (uint32_t)second;
        cmd.d[9] = (uint32_t)(second >> 32);
    } else {
        uint32_t n = pages - 1u;
        if (n > 512u) return 0;
        for (uint32_t i = 0; i < n; i++)
            g_prp_list[i] = first_page + (uint64_t)(i + 1u) * NVME_PAGE_SIZE;
        uint64_t list = phys(g_prp_list);
        cmd.d[8] = (uint32_t)list;
        cmd.d[9] = (uint32_t)(list >> 32);
    }

    /* Namespace ID 1 is selected by the caller's supported scope. */
    cmd.d[1] = 1u;
    cmd.d[0] &= 0xFFFFu;
    cmd.d[0] |= (uint32_t)g_cid++ << 16;

    g_io_sq[g_iosq_tail] = cmd;
    g_iosq_tail = (uint16_t)((g_iosq_tail + 1) % NVME_QDEPTH);
    ring_sq(1, g_iosq_tail);

    for (uint32_t i = 0; i < NVME_IO_TIMEOUT; i++) {
        nvme_cqe_t *cqe = &g_io_cq[g_iocq_head];
        if ((cqe->status & 1u) == g_iocq_phase) {
            int ok = completion_ok(cqe);
            g_iocq_head = (uint16_t)((g_iocq_head + 1) % NVME_QDEPTH);
            if (g_iocq_head == 0) g_iocq_phase ^= 1u;
            ring_cq(1, g_iocq_head);
            return ok;
        }
    }
    return 0;
}

static int identify_controller(void) {
    nvme_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.d[0] = NVME_ADMIN_IDENTIFY;
    cmd.d[1] = 0;
    cmd.d[6] = (uint32_t)phys(g_identify);
    cmd.d[7] = (uint32_t)(phys(g_identify) >> 32);
    cmd.d[10] = 1u; /* CNS = Identify Controller */

    if (!admin_submit(&cmd)) return 0;
    /* PCI VID/DID are the authoritative controller identifiers here. */
    return 1;
}

static int identify_namespace(void) {
    nvme_cmd_t cmd;
    memset(&cmd, 0, sizeof(cmd));
    memset(g_identify, 0, sizeof(g_identify));
    cmd.d[0] = NVME_ADMIN_IDENTIFY;
    cmd.d[1] = 1u; /* namespace ID 1 */
    cmd.d[6] = (uint32_t)phys(g_identify);
    cmd.d[7] = (uint32_t)(phys(g_identify) >> 32);
    cmd.d[10] = 0u; /* CNS = Identify Namespace */

    if (!admin_submit(&cmd)) return 0;

    uint64_t nsze = 0;
    for (int i = 0; i < 8; i++) nsze |= (uint64_t)g_identify[i] << (i * 8);
    if (nsze == 0) return 0;

    uint8_t flbas = g_identify[26];
    uint8_t format = flbas & 0x0Fu;
    uint32_t off = 128u + (uint32_t)format * 16u;
    if (off + 4u > sizeof(g_identify)) return 0;
    uint8_t lbads = g_identify[off + 2u];
    if (lbads >= 31u) return 0;
    g_sector_size = 1u << lbads;
    g_ns_sectors = nsze;

    /* The current block API is expressed in 512-byte sectors. */
    return g_sector_size == 512u;
}

static int create_io_queues(void) {
    nvme_cmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.d[0] = NVME_ADMIN_CREATE_IO_CQ;
    cmd.d[6] = (uint32_t)phys(g_io_cq);
    cmd.d[7] = (uint32_t)(phys(g_io_cq) >> 32);
    cmd.d[10] = (NVME_QDEPTH - 1u) << 16 | 1u;
    cmd.d[11] = 1u; /* physically contiguous, no interrupts */
    if (!admin_submit(&cmd)) return 0;

    memset(&cmd, 0, sizeof(cmd));
    cmd.d[0] = NVME_ADMIN_CREATE_IO_SQ;
    cmd.d[6] = (uint32_t)phys(g_io_sq);
    cmd.d[7] = (uint32_t)(phys(g_io_sq) >> 32);
    cmd.d[10] = (NVME_QDEPTH - 1u) << 16 | 1u;
    cmd.d[11] = 1u; /* CQID 1 */
    if (!admin_submit(&cmd)) return 0;

    return 1;
}

int nvme_init(void) {
    g_ready = 0;
    g_sector_size = 0;
    g_ns_sectors = 0;
    g_cid = 1;
    g_asq_tail = 0;
    g_acq_head = 0;
    g_acq_phase = 1;
    g_iosq_tail = 0;
    g_iocq_head = 0;
    g_iocq_phase = 1;

    pci_scan();
    nexus_pci_device_t dev;
    if (!pci_find_class(NVME_CLASS, NVME_SUBCLASS, NVME_PROGIF, &dev)) return 0;

    uint64_t bar = pci_get_bar64(&dev, 0);
    if (!bar) return 0;
    pci_enable_device(&dev, 1, 1);

    g_vendor_id = dev.vendor_id;
    g_device_id = dev.device_id;
    g_regs = (volatile uint8_t *)(uintptr_t)bar;
    g_cap = mmio64(NVME_REG_CAP);
    g_db_stride = 4u << ((uint32_t)(g_cap >> 32) & 0xFu);
    if (g_db_stride == 0 || g_db_stride > 4096u) return 0;

    uint32_t vs = mmio32(NVME_REG_VS);
    g_vs_major = (uint8_t)((vs >> 16) & 0xFFu);
    g_vs_minor = (uint8_t)((vs >> 8) & 0xFFu);

    /* Disable the controller before replacing its admin queues. */
    uint32_t cc = mmio32(NVME_REG_CC);
    cc &= ~NVME_CC_EN;
    mmio32w(NVME_REG_CC, cc);
    if (!wait_reg(NVME_REG_CSTS, NVME_CSTS_RDY, 0, NVME_ADMIN_TIMEOUT)) return 0;

    memset(g_admin_sq, 0, sizeof(g_admin_sq));
    memset(g_admin_cq, 0, sizeof(g_admin_cq));
    memset(g_io_sq, 0, sizeof(g_io_sq));
    memset(g_io_cq, 0, sizeof(g_io_cq));
    memset(g_prp_list, 0, sizeof(g_prp_list));
    memset(g_identify, 0, sizeof(g_identify));

    mmio32w(NVME_REG_AQA, ((NVME_QDEPTH - 1u) << 16) | (NVME_QDEPTH - 1u));
    *(volatile uint64_t *)(g_regs + NVME_REG_ASQ) = phys(g_admin_sq);
    *(volatile uint64_t *)(g_regs + NVME_REG_ACQ) = phys(g_admin_cq);

    cc = NVME_CC_EN | NVME_CC_CSS_NVM | NVME_CC_MPS_4K |
         NVME_CC_IOSQES | NVME_CC_IOCQES;
    mmio32w(NVME_REG_CC, cc);
    if (!wait_reg(NVME_REG_CSTS, NVME_CSTS_RDY, NVME_CSTS_RDY, NVME_ADMIN_TIMEOUT)) return 0;
    if (mmio32(NVME_REG_CSTS) & NVME_CSTS_CFS) return 0;

    if (!identify_controller()) return 0;
    if (!identify_namespace()) return 0;
    if (!create_io_queues()) return 0;

    g_ready = 1;
    return 1;
}

int nvme_is_ready(void) { return g_ready; }

int nvme_read_sectors(uint64_t lba, uint32_t count, void *buf) {
    if (!g_ready || !buf || count == 0 || count > NVME_MAX_SECTORS) return 0;
    if (lba >= g_ns_sectors || count > g_ns_sectors - lba) return 0;
    if ((uintptr_t)buf + (uint64_t)count * 512u < (uintptr_t)buf) return 0;
    return io_submit_read(lba, count, buf);
}

uint32_t nvme_sector_size(void) { return g_sector_size; }
uint64_t nvme_namespace_sectors(void) { return g_ns_sectors; }
uint16_t nvme_controller_vendor_id(void) { return g_vendor_id; }
uint16_t nvme_controller_device_id(void) { return g_device_id; }
uint8_t nvme_version_major(void) { return g_vs_major; }
uint8_t nvme_version_minor(void) { return g_vs_minor; }
