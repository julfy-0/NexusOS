/* NexusOS: минимальный AHCI-драйвер. См. предупреждения в ahci.h.
 *
 * Все структуры регистров/команд построены строго по AHCI spec 1.3.1,
 * без битовых полей в C (bitfields) — их раскладка в памяти не гарантирована
 * стандартом и отличается между компиляторами, поэтому DW-слова собираются
 * вручную через сдвиги. Так же поступают в реальных ОС-драйверах. */
#include "ahci.h"
#include "pci.h"

extern void *memset(void *dest, int value, unsigned long n);

/* ---- Регистры HBA (AHCI spec, раздел 3) ---- */

typedef struct {
    volatile uint32_t clb;
    volatile uint32_t clbu;
    volatile uint32_t fb;
    volatile uint32_t fbu;
    volatile uint32_t is;
    volatile uint32_t ie;
    volatile uint32_t cmd;
    volatile uint32_t reserved0;
    volatile uint32_t tfd;
    volatile uint32_t sig;
    volatile uint32_t ssts;
    volatile uint32_t sctl;
    volatile uint32_t serr;
    volatile uint32_t sact;
    volatile uint32_t ci;
    volatile uint32_t sntf;
    volatile uint32_t fbs;
    volatile uint32_t reserved1[11];
    volatile uint32_t vendor[4];
} hba_port_t; /* 0x80 байт */

typedef struct {
    volatile uint32_t cap;
    volatile uint32_t ghc;
    volatile uint32_t is;
    volatile uint32_t pi;
    volatile uint32_t vs;
    volatile uint32_t ccc_ctl;
    volatile uint32_t ccc_pts;
    volatile uint32_t em_loc;
    volatile uint32_t em_ctl;
    volatile uint32_t cap2;
    volatile uint32_t bohc;
    uint8_t  reserved[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    hba_port_t ports[32];
} hba_mem_t;

#define GHC_AE   (1u << 31)
#define PXCMD_ST (1u << 0)
#define PXCMD_FRE (1u << 4)
#define PXCMD_FR  (1u << 14)
#define PXCMD_CR  (1u << 15)
#define PXIS_TFES (1u << 30)
#define PXTFD_ERR 0x01
#define PXTFD_BSY 0x80
#define PXTFD_DRQ 0x08

#define SATA_SIG_ATA 0x00000101u

/* ---- Командные структуры (AHCI spec, раздел 4) ---- */

typedef struct {
    uint32_t dw0; /* CFL(0-4) A(5) W(6) P(7) R(8) B(9) C(10) PMP(12-15) PRDTL(16-31) */
    volatile uint32_t prdbc;
    uint32_t ctba;
    uint32_t ctbau;
    uint32_t reserved[4];
} hba_cmd_header_t; /* 32 байта */

typedef struct {
    uint32_t dba;
    uint32_t dbau;
    uint32_t reserved0;
    uint32_t dbc_ic; /* bits0-21 byte count-1, bit31 interrupt on completion */
} hba_prdt_entry_t; /* 16 байт */

typedef struct {
    uint8_t cfis[64];
    uint8_t acmd[16];
    uint8_t reserved[48];
    hba_prdt_entry_t prdt[1]; /* нам хватает одной записи на команду */
} hba_cmd_table_t;

/* ---- Статические буферы под один порт (нет кучи — используем .bss) ---- */

static uint8_t g_port_clb[1024] __attribute__((aligned(1024)));
static uint8_t g_port_fb[256] __attribute__((aligned(256)));
static uint8_t g_cmd_table[256] __attribute__((aligned(128)));

static volatile hba_mem_t *g_hba = 0;
static int g_port_index = -1;
static int g_ready = 0;

#define SPIN_LIMIT 10000000u /* грубый bounded spin вместо аппаратного таймаута */

static int wait_while(volatile uint32_t *reg, uint32_t mask) {
    for (uint32_t i = 0; i < SPIN_LIMIT; i++) {
        if ((*reg & mask) == 0) return 1;
    }
    return 0; /* таймаут */
}

int ahci_init(void) {
    g_ready = 0;

    pci_scan();

    nexus_pci_device_t dev;
    if (!pci_find_class(0x01, 0x06, 0x01, &dev)) {
        return 0; /* нет AHCI-контроллера на шине */
    }

    uint32_t abar = dev.bar[5] & 0xFFFFFFF0u;
    if (abar == 0) {
        return 0;
    }

    g_hba = (volatile hba_mem_t *)(uintptr_t)abar;
    g_hba->ghc |= GHC_AE;

    int chosen = -1;
    for (int i = 0; i < 32; i++) {
        if (!(g_hba->pi & (1u << i))) continue;

        uint32_t ssts = g_hba->ports[i].ssts;
        uint32_t det = ssts & 0xF;
        if (det != 3) continue; /* нет устройства / не поднялась PHY-связь */

        if (g_hba->ports[i].sig != SATA_SIG_ATA) continue; /* не обычный SATA-диск */

        chosen = i;
        break;
    }

    if (chosen < 0) {
        return 0;
    }

    volatile hba_port_t *port = &g_hba->ports[chosen];

    /* Останавливаем движок команд перед перенастройкой (обязательно по spec) */
    port->cmd &= ~PXCMD_ST;
    wait_while(&port->cmd, PXCMD_CR);
    port->cmd &= ~PXCMD_FRE;
    wait_while(&port->cmd, PXCMD_FR);

    memset(g_port_clb, 0, sizeof(g_port_clb));
    memset(g_port_fb, 0, sizeof(g_port_fb));
    memset(g_cmd_table, 0, sizeof(g_cmd_table));

    port->clb = (uint32_t)(uintptr_t)g_port_clb;
    port->clbu = 0;
    port->fb = (uint32_t)(uintptr_t)g_port_fb;
    port->fbu = 0;

    port->serr = 0xFFFFFFFFu; /* сброс всех бит SATA error (write-1-to-clear) */
    port->ie = 0;             /* прерывания не используем, работаем через polling */

    port->cmd |= PXCMD_FRE;

    /* Ждём, пока диск не освободится (BSY/DRQ), но не бесконечно — если
     * что-то не так, лучше продолжить и словить ошибку на первой команде,
     * чем зависнуть на старте намертво. */
    for (uint32_t i = 0; i < SPIN_LIMIT; i++) {
        if ((port->tfd & (PXTFD_BSY | PXTFD_DRQ)) == 0) break;
    }

    port->cmd |= PXCMD_ST;

    g_port_index = chosen;
    g_ready = 1;
    return 1;
}

int ahci_is_ready(void) {
    return g_ready;
}

int ahci_read_sectors(uint64_t lba, uint32_t count, void *buf) {
    if (!g_ready || count == 0 || count > 128) {
        return 0; /* 128 секторов = 64 КБ за один вызов, с запасом для наших нужд */
    }

    volatile hba_port_t *port = &g_hba->ports[g_port_index];

    /* Ждём, пока предыдущая команда (если была) точно завершилась. */
    if (!wait_while(&port->tfd, PXTFD_BSY | PXTFD_DRQ)) {
        return 0;
    }

    hba_cmd_header_t *hdr = (hba_cmd_header_t *)g_port_clb;
    hdr->dw0 = 5u /* CFL = 5 DWORD (20 байт H2D FIS) */ | (1u << 16) /* PRDTL = 1 */;
    hdr->prdbc = 0;
    hdr->ctba = (uint32_t)(uintptr_t)g_cmd_table;
    hdr->ctbau = 0;

    hba_cmd_table_t *tbl = (hba_cmd_table_t *)g_cmd_table;
    memset(tbl, 0, sizeof(hba_cmd_table_t));

    tbl->prdt[0].dba = (uint32_t)(uintptr_t)buf;
    tbl->prdt[0].dbau = 0;
    tbl->prdt[0].dbc_ic = (count * 512u - 1u) & 0x3FFFFFu;

    uint8_t *fis = tbl->cfis;
    fis[0] = 0x27; /* FIS_TYPE_REG_H2D */
    fis[1] = 0x80; /* C=1 (command), PM port 0 */
    fis[2] = 0x25; /* READ DMA EXT */
    fis[3] = 0;
    fis[4] = (uint8_t)(lba & 0xFF);
    fis[5] = (uint8_t)((lba >> 8) & 0xFF);
    fis[6] = (uint8_t)((lba >> 16) & 0xFF);
    fis[7] = 0x40; /* Device: LBA mode */
    fis[8] = (uint8_t)((lba >> 24) & 0xFF);
    fis[9] = (uint8_t)((lba >> 32) & 0xFF);
    fis[10] = (uint8_t)((lba >> 40) & 0xFF);
    fis[11] = 0;
    fis[12] = (uint8_t)(count & 0xFF);
    fis[13] = (uint8_t)((count >> 8) & 0xFF);
    fis[14] = 0;
    fis[15] = 0;

    port->is = 0xFFFFFFFFu; /* сброс предыдущего статуса перед новой командой */
    port->ci |= 1u;         /* "звоним в дверь" — слот 0 */

    for (uint32_t i = 0; i < SPIN_LIMIT; i++) {
        if (port->is & PXIS_TFES) {
            return 0; /* Task File Error */
        }
        if (!(port->ci & 1u)) {
            break; /* команда завершена */
        }
        if (i == SPIN_LIMIT - 1) {
            return 0; /* таймаут */
        }
    }

    if (port->tfd & PXTFD_ERR) {
        return 0;
    }

    return 1;
}
