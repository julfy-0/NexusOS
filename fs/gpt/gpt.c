#include "gpt.h"
#include "ahci.h"

extern void *memset(void *dest, int value, unsigned long n);

#define GPT_HEADER_LBA 1
#define GPT_SIGNATURE 0x5452415020494645ULL /* "EFI PART" little-endian */
#define GPT_ENTRY_SIZE 128
#define GPT_HEADER_SIZE 92

static nexus_gpt_partition_t g_parts[NEXUS_GPT_MAX_PARTITIONS];
static int g_count;
static uint8_t g_sector[512];

static int hex(uint8_t v) { return v < 10 ? '0' + v : 'A' + v - 10; }

static void guid_to_text(const uint8_t *g, char out[33]) {
    /* GPT stores the first three GUID fields little-endian. */
    static const int order[16] = {3,2,1,0,5,4,7,6,8,9,10,11,12,13,14,15};
    int j = 0;
    for (int i = 0; i < 16; ++i) {
        uint8_t v = g[order[i]];
        out[j++] = (char)hex(v >> 4);
        out[j++] = (char)hex(v & 15);
    }
    out[32] = '\0';
}

static int guid_zero(const uint8_t *g) {
    for (int i = 0; i < 16; ++i) if (g[i]) return 0;
    return 1;
}

static int str_eq(const char *a, const char *b) {
    int i = 0; while (a[i] && b[i] && a[i] == b[i]) ++i;
    return a[i] == 0 && b[i] == 0;
}

static uint64_t le64(const uint8_t *p) {
    uint64_t v = 0; for (int i=0;i<8;i++) v |= (uint64_t)p[i] << (i*8); return v;
}

static void decode_name(const uint8_t *p, char *out, int max) {
    int n = 0;
    for (int i=0; i<36 && n<max-1; i+=2) {
        uint16_t c = (uint16_t)p[i] | ((uint16_t)p[i+1] << 8);
        if (!c) break;
        out[n++] = (c < 128) ? (char)c : '?';
    }
    out[n] = '\0';
}

int gpt_scan(void) {
    g_count = 0;
    memset(g_parts, 0, sizeof(g_parts));
    if (!ahci_is_ready()) return 0;
    if (!ahci_read_sectors(GPT_HEADER_LBA, 1, g_sector)) return 0;
    if (le64(g_sector) != GPT_SIGNATURE) return 0;

    uint32_t header_size = (uint32_t)g_sector[12] | ((uint32_t)g_sector[13]<<8) |
                           ((uint32_t)g_sector[14]<<16) | ((uint32_t)g_sector[15]<<24);
    if (header_size < GPT_HEADER_SIZE || header_size > 512) return 0;
    uint64_t entries_lba = le64(g_sector + 72);
    uint32_t entry_count = (uint32_t)g_sector[80] | ((uint32_t)g_sector[81]<<8) |
                           ((uint32_t)g_sector[82]<<16) | ((uint32_t)g_sector[83]<<24);
    uint32_t entry_size = (uint32_t)g_sector[84] | ((uint32_t)g_sector[85]<<8) |
                          ((uint32_t)g_sector[86]<<16) | ((uint32_t)g_sector[87]<<24);
    if (!entry_count || entry_size < GPT_ENTRY_SIZE || entry_size > 512) return 0;

    uint8_t entry_sector[1024];
    for (uint32_t i=0; i<entry_count && g_count<NEXUS_GPT_MAX_PARTITIONS; ++i) {
        uint64_t byte_off = (uint64_t)i * entry_size;
        uint64_t lba = entries_lba + byte_off / 512;
        uint32_t off = (uint32_t)(byte_off % 512);
        if (off + GPT_ENTRY_SIZE > 512) {
            if (!ahci_read_sectors(lba, 2, entry_sector)) break;
            off = (uint32_t)(byte_off % 512);
        } else if (!ahci_read_sectors(lba, 1, entry_sector)) break;
        const uint8_t *e = entry_sector + off;
        if (guid_zero(e)) continue;
        nexus_gpt_partition_t *p = &g_parts[g_count++];
        p->index = i + 1;
        p->first_lba = le64(e + 32);
        p->last_lba = le64(e + 40);
        p->attributes = le64(e + 48);
        guid_to_text(e, p->type_guid);
        decode_name(e + 56, p->name, sizeof(p->name));
    }
    return g_count;
}

int gpt_partition_count(void) { return g_count; }
const nexus_gpt_partition_t *gpt_partition(int index) {
    if (index < 0 || index >= g_count) return 0;
    return &g_parts[index];
}
const nexus_gpt_partition_t *gpt_find_type(const char *type_guid) {
    for (int i=0;i<g_count;i++) if (str_eq(g_parts[i].type_guid, type_guid)) return &g_parts[i];
    return 0;
}
const nexus_gpt_partition_t *gpt_find_name(const char *name) {
    for (int i=0;i<g_count;i++) if (str_eq(g_parts[i].name, name)) return &g_parts[i];
    return 0;
}
