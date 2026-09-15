/* NexusOS: FAT32 read/write foundation. См. ограничения в fat32.h. */
#include "fat32.h"
#include "ahci.h"
#include "console.h"

extern void *memset(void *dest, int value, unsigned long n);
extern void *memcpy(void *dest, const void *src, unsigned long n);
extern int strncmp(const char *a, const char *b, unsigned long n);

/* Boot Parameter Block FAT32 — раскладка строго как на диске, поэтому
 * структура packed (без выравнивания/паддинга от компилятора). */
typedef struct {
    uint8_t  jmp[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries16;
    uint16_t total_sectors16;
    uint8_t  media;
    uint16_t fat_size16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors32;
    uint32_t fat_size32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved0[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} __attribute__((packed)) fat32_bpb_t;

typedef struct {
    uint8_t  name[11]; /* 8.3, без точки, пробелами дополнено */
    uint8_t  attr;
    uint8_t  nt_reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_hi;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_lo;
    uint32_t file_size;
} __attribute__((packed)) fat32_dirent_t;

#define ATTR_DIRECTORY 0x10
#define ATTR_LFN       0x0F
#define ATTR_VOLUME_ID 0x08

#define MAX_SECTORS_PER_READ 128 /* ограничение ahci_read_sectors, см. ahci.c */

static int memcmp_local(const void *a, const void *b, int n);

static uint64_t g_partition_lba;
static uint32_t g_bytes_per_sector;
static uint32_t g_sectors_per_cluster;
static uint32_t g_root_cluster;
static uint32_t g_max_cluster;
static uint32_t g_fat_count;
static uint32_t g_fat_size_sectors;
static uint64_t g_fat_start_lba;
static uint64_t g_cluster_heap_start_lba;
static int g_mounted = 0;

/* Общий рабочий буфер под содержимое кластера/директории. Кластер не может
 * быть больше MAX_SECTORS_PER_READ секторов — это же ограничение диктует
 * ahci_read_sectors(), так что размер буфера всегда достаточен. */
static uint8_t g_cluster_buf[MAX_SECTORS_PER_READ * 512];
static uint8_t g_fat_buf[512];

static uint64_t cluster_to_lba(uint32_t cluster) {
    return g_cluster_heap_start_lba + (uint64_t)(cluster - 2) * g_sectors_per_cluster;
}

static int read_cluster(uint32_t cluster) {
    return ahci_read_sectors(cluster_to_lba(cluster), g_sectors_per_cluster, g_cluster_buf);
}

/* 0x0FFFFFF8..0x0FFFFFFF = конец цепочки, 0x0FFFFFF7 = плохой кластер */
static uint32_t get_next_cluster(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint64_t fat_sector = g_fat_start_lba + (fat_offset / g_bytes_per_sector);
    uint32_t ent_offset = fat_offset % g_bytes_per_sector;

    if (!ahci_read_sectors(fat_sector, 1, g_fat_buf)) {
        return 0x0FFFFFFF; /* трактуем ошибку чтения как конец цепочки */
    }

    uint32_t value = (uint32_t)g_fat_buf[ent_offset] |
                      ((uint32_t)g_fat_buf[ent_offset + 1] << 8) |
                      ((uint32_t)g_fat_buf[ent_offset + 2] << 16) |
                      ((uint32_t)g_fat_buf[ent_offset + 3] << 24);
    return value & 0x0FFFFFFF;
}

static int is_end_of_chain(uint32_t cluster) {
    return cluster >= 0x0FFFFFF8;
}

/* Превращает "kernel.elf" в "KERNEL  ELF" (11 байт, без точки, пробелами
 * дополнено, верхний регистр). Имена длиннее 8.3 обрезаются — такие файлы
 * можно будет найти только по их короткому имени на диске (см. fat32.h). */
static void to_short_name(const char *input, uint8_t out[11]) {
    for (int i = 0; i < 11; i++) out[i] = ' ';

    int i = 0, out_pos = 0;
    while (input[i] != '\0' && input[i] != '.' && out_pos < 8) {
        char c = input[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out[out_pos++] = (uint8_t)c;
        i++;
    }
    while (input[i] != '\0' && input[i] != '.') i++; /* пропускаем остаток имени, если оно длиннее 8 */
    if (input[i] == '.') {
        i++;
        int ext_pos = 8;
        while (input[i] != '\0' && ext_pos < 11) {
            char c = input[i];
            if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            out[ext_pos++] = (uint8_t)c;
            i++;
        }
    }
}

int fat32_mount(uint64_t partition_lba) {
    g_mounted = 0;

    static uint8_t vbr[512];
    if (!ahci_read_sectors(partition_lba, 1, vbr)) {
        return 0;
    }

    if (vbr[510] != 0x55 || vbr[511] != 0xAA) {
        return 0; /* нет сигнатуры загрузочного сектора */
    }

    fat32_bpb_t *bpb = (fat32_bpb_t *)vbr;

    if (bpb->fat_size16 != 0 || bpb->fat_size32 == 0) {
        return 0; /* это не FAT32 (скорее всего FAT12/16, у них fat_size16 != 0) */
    }
    if (bpb->bytes_per_sector != 512) {
        return 0; /* упрощение драйвера, см. fat32.h */
    }
    if (bpb->sectors_per_cluster == 0 ||
        bpb->sectors_per_cluster > MAX_SECTORS_PER_READ) {
        return 0;
    }

    g_partition_lba = partition_lba;
    g_bytes_per_sector = bpb->bytes_per_sector;
    g_sectors_per_cluster = bpb->sectors_per_cluster;
    g_root_cluster = bpb->root_cluster;
    g_fat_count = bpb->num_fats;
    g_fat_size_sectors = bpb->fat_size32;
    g_fat_start_lba = partition_lba + bpb->reserved_sectors;
    g_cluster_heap_start_lba = g_fat_start_lba + (uint64_t)bpb->num_fats * bpb->fat_size32;

    /* Maximum valid data-cluster number. Cluster 2 is the first data cluster. */
    uint64_t data_sectors = bpb->total_sectors32;
    if (data_sectors < bpb->reserved_sectors + (uint32_t)bpb->num_fats * bpb->fat_size32) return 0;
    data_sectors -= bpb->reserved_sectors + (uint32_t)bpb->num_fats * bpb->fat_size32;
    uint64_t cluster_count = data_sectors / bpb->sectors_per_cluster;
    if (cluster_count < 1 || cluster_count > 0x0FFFFFF5u) return 0;
    g_max_cluster = (uint32_t)cluster_count + 1u;

    g_mounted = 1;
    return 1;
}

int fat32_is_mounted(void) {
    return g_mounted;
}

/* Ищет entry с именем name (уже в формате 8.3) среди детей директории,
 * начинающейся с dir_cluster. Возвращает 1 и заполняет *out при успехе. */
static int find_entry(uint32_t dir_cluster, const uint8_t name[11], fat32_dirent_t *out) {
    uint32_t cluster = dir_cluster;

    while (!is_end_of_chain(cluster) && cluster >= 2) {
        if (!read_cluster(cluster)) return 0;

        uint32_t entries_per_cluster = (g_sectors_per_cluster * g_bytes_per_sector) / sizeof(fat32_dirent_t);
        fat32_dirent_t *entries = (fat32_dirent_t *)g_cluster_buf;

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            uint8_t first = entries[i].name[0];
            if (first == 0x00) return 0; /* конец директории */
            if (first == 0xE5) continue;  /* удалённая запись */
            if (entries[i].attr == ATTR_LFN) continue;
            if (entries[i].attr & ATTR_VOLUME_ID) continue;

            if (memcmp_local(entries[i].name, name, 11) == 0) {
                *out = entries[i];
                return 1;
            }
        }

        cluster = get_next_cluster(cluster);
    }

    return 0;
}

/* Свой memcmp на 11 байт — не тащим лишнюю extern-зависимость ради одного
 * маленького сравнения. */
static int memcmp_local(const void *a, const void *b, int n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (int i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

/* Разбирает path вида "/foo/bar.txt" на компоненты и идёт по дереву от
 * корня. Возвращает 1 и заполняет *out, если путь найден. Если путь — это
 * ровно "/", возвращает виртуальную "корневую" запись (is_root=1 через
 * first_cluster==root_cluster и attr=ATTR_DIRECTORY). */
static int resolve_path(const char *path, fat32_dirent_t *out) {
    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        memset(out, 0, sizeof(*out));
        out->attr = ATTR_DIRECTORY;
        out->first_cluster_hi = (uint16_t)(g_root_cluster >> 16);
        out->first_cluster_lo = (uint16_t)(g_root_cluster & 0xFFFF);
        return 1;
    }

    uint32_t current_cluster = g_root_cluster;
    int pos = (path[0] == '/') ? 1 : 0;
    fat32_dirent_t entry;

    while (path[pos] != '\0') {
        char component[13]; /* 8+1+3+NUL */
        int clen = 0;
        while (path[pos] != '\0' && path[pos] != '/' && clen < 12) {
            component[clen++] = path[pos++];
        }
        component[clen] = '\0';
        while (path[pos] == '/') pos++;

        uint8_t short_name[11];
        to_short_name(component, short_name);

        if (!find_entry(current_cluster, short_name, &entry)) {
            return 0;
        }

        if (path[pos] != '\0') {
            /* Есть ещё компоненты пути — текущая запись должна быть директорией */
            if (!(entry.attr & ATTR_DIRECTORY)) return 0;
            current_cluster = ((uint32_t)entry.first_cluster_hi << 16) | entry.first_cluster_lo;
        }
    }

    *out = entry;
    return 1;
}

int fat32_is_directory(const char *path) {
    if (!g_mounted) return 0;
    fat32_dirent_t entry;
    if (!resolve_path(path, &entry)) return 0;
    return (entry.attr & ATTR_DIRECTORY) != 0;
}

int fat32_list(const char *path) {
    if (!g_mounted) {
        console_print("fat32: not mounted\n");
        return 0;
    }

    fat32_dirent_t dir_entry;
    if (!resolve_path(path, &dir_entry)) {
        console_print("fat32: path not found\n");
        return 0;
    }
    if (!(dir_entry.attr & ATTR_DIRECTORY)) {
        console_print("fat32: not a directory\n");
        return 0;
    }

    uint32_t cluster = ((uint32_t)dir_entry.first_cluster_hi << 16) | dir_entry.first_cluster_lo;
    if (cluster == 0) cluster = g_root_cluster; /* некоторые ФС кодируют корень как 0 */

    int any = 0;

    while (!is_end_of_chain(cluster) && cluster >= 2) {
        if (!read_cluster(cluster)) break;

        uint32_t entries_per_cluster = (g_sectors_per_cluster * g_bytes_per_sector) / sizeof(fat32_dirent_t);
        fat32_dirent_t *entries = (fat32_dirent_t *)g_cluster_buf;

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            uint8_t first = entries[i].name[0];
            if (first == 0x00) goto done;
            if (first == 0xE5) continue;
            if (entries[i].attr == ATTR_LFN) continue;
            if (entries[i].attr & ATTR_VOLUME_ID) continue;

            /* Печатаем имя как есть (8.3, с пробелами) — просто и честно. */
            char name_buf[13];
            int np = 0;
            for (int c = 0; c < 8 && entries[i].name[c] != ' '; c++) name_buf[np++] = (char)entries[i].name[c];
            if (entries[i].name[8] != ' ') {
                name_buf[np++] = '.';
                for (int c = 8; c < 11 && entries[i].name[c] != ' '; c++) name_buf[np++] = (char)entries[i].name[c];
            }
            name_buf[np] = '\0';

            console_print(name_buf);
            if (entries[i].attr & ATTR_DIRECTORY) {
                console_print("/\n");
            } else {
                console_print("  (");
                console_print_dec(entries[i].file_size);
                console_print(" bytes)\n");
            }
            any = 1;
        }

        cluster = get_next_cluster(cluster);
    }

done:
    if (!any) {
        console_print("(empty)\n");
    }
    return 1;
}

int fat32_list_entries(const char *path, char names[][13], unsigned char is_dir[], int max_entries) {
    if (!g_mounted || !names || !is_dir || max_entries <= 0) return -1;

    fat32_dirent_t dir_entry;
    if (!resolve_path(path, &dir_entry) || !(dir_entry.attr & ATTR_DIRECTORY)) return -1;

    uint32_t cluster = ((uint32_t)dir_entry.first_cluster_hi << 16) | dir_entry.first_cluster_lo;
    if (cluster == 0) cluster = g_root_cluster;

    int count = 0;
    while (!is_end_of_chain(cluster) && cluster >= 2 && count < max_entries) {
        if (!read_cluster(cluster)) break;
        uint32_t entries_per_cluster = (g_sectors_per_cluster * g_bytes_per_sector) / sizeof(fat32_dirent_t);
        fat32_dirent_t *entries = (fat32_dirent_t *)g_cluster_buf;

        for (uint32_t i = 0; i < entries_per_cluster && count < max_entries; ++i) {
            uint8_t first = entries[i].name[0];
            if (first == 0x00) return count;
            if (first == 0xE5 || entries[i].attr == ATTR_LFN || (entries[i].attr & ATTR_VOLUME_ID)) continue;

            int np = 0;
            for (int c = 0; c < 8 && entries[i].name[c] != ' '; ++c)
                names[count][np++] = (char)entries[i].name[c];
            if (entries[i].name[8] != ' ') {
                names[count][np++] = '.';
                for (int c = 8; c < 11 && entries[i].name[c] != ' '; ++c)
                    names[count][np++] = (char)entries[i].name[c];
            }
            names[count][np] = '\0';
            is_dir[count] = (entries[i].attr & ATTR_DIRECTORY) ? 1 : 0;
            ++count;
        }
        cluster = get_next_cluster(cluster);
    }
    return count;
}

int fat32_read_file(const char *path, void *buf, uint32_t buf_size, uint32_t *out_size) {
    if (!g_mounted) return 0;

    fat32_dirent_t entry;
    if (!resolve_path(path, &entry)) return 0;
    if (entry.attr & ATTR_DIRECTORY) return 0;

    uint32_t cluster = ((uint32_t)entry.first_cluster_hi << 16) | entry.first_cluster_lo;
    uint32_t remaining = entry.file_size;
    uint32_t written = 0;
    uint32_t cluster_bytes = g_sectors_per_cluster * g_bytes_per_sector;
    uint8_t *dst = (uint8_t *)buf;

    while (!is_end_of_chain(cluster) && cluster >= 2 && remaining > 0 && written < buf_size) {
        if (!read_cluster(cluster)) break;

        uint32_t chunk = cluster_bytes;
        if (chunk > remaining) chunk = remaining;
        if (written + chunk > buf_size) chunk = buf_size - written;

        memcpy(dst + written, g_cluster_buf, chunk);
        written += chunk;
        remaining -= chunk;

        cluster = get_next_cluster(cluster);
    }

    if (out_size) *out_size = written;
    return 1;
}


/* ---- Writable FAT32 foundation ----------------------------------------- */

static uint32_t dirent_cluster(const fat32_dirent_t *e) {
    return ((uint32_t)e->first_cluster_hi << 16) | e->first_cluster_lo;
}

static void set_dirent_cluster(fat32_dirent_t *e, uint32_t cluster) {
    e->first_cluster_hi = (uint16_t)(cluster >> 16);
    e->first_cluster_lo = (uint16_t)(cluster & 0xFFFFu);
}

static int fat32_read_entry(uint32_t cluster, uint32_t *out) {
    if (!out || cluster < 2 || cluster > g_max_cluster) return 0;
    uint32_t fat_offset = cluster * 4u;
    uint64_t sector = g_fat_start_lba + fat_offset / g_bytes_per_sector;
    uint32_t off = fat_offset % g_bytes_per_sector;
    if (off > 508u || !ahci_read_sectors(sector, 1, g_fat_buf)) return 0;
    *out = ((uint32_t)g_fat_buf[off]) |
           ((uint32_t)g_fat_buf[off + 1] << 8) |
           ((uint32_t)g_fat_buf[off + 2] << 16) |
           ((uint32_t)g_fat_buf[off + 3] << 24);
    *out &= 0x0FFFFFFFu;
    return 1;
}

static int fat32_write_entry_one(uint64_t fat_lba, uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4u;
    uint64_t sector = fat_lba + fat_offset / g_bytes_per_sector;
    uint32_t off = fat_offset % g_bytes_per_sector;
    if (off > 508u || !ahci_read_sectors(sector, 1, g_fat_buf)) return 0;
    uint32_t old = (uint32_t)g_fat_buf[off] |
                   ((uint32_t)g_fat_buf[off + 1] << 8) |
                   ((uint32_t)g_fat_buf[off + 2] << 16) |
                   ((uint32_t)g_fat_buf[off + 3] << 24);
    uint32_t merged = (old & 0xF0000000u) | (value & 0x0FFFFFFFu);
    g_fat_buf[off] = (uint8_t)merged;
    g_fat_buf[off + 1] = (uint8_t)(merged >> 8);
    g_fat_buf[off + 2] = (uint8_t)(merged >> 16);
    g_fat_buf[off + 3] = (uint8_t)(merged >> 24);
    return ahci_write_sectors(sector, 1, g_fat_buf);
}

static int fat32_set_entry(uint32_t cluster, uint32_t value) {
    if (!fat32_write_entry_one(g_fat_start_lba, cluster, value)) return 0;
    for (uint32_t i = 1; i < g_fat_count; ++i) {
        uint64_t lba = g_fat_start_lba + (uint64_t)i * g_fat_size_sectors;
        if (!fat32_write_entry_one(lba, cluster, value)) return 0;
    }
    return 1;
}

static int fat32_zero_cluster(uint32_t cluster) {
    uint32_t sectors = g_sectors_per_cluster;
    memset(g_cluster_buf, 0, sectors * g_bytes_per_sector);
    return ahci_write_sectors(cluster_to_lba(cluster), sectors, g_cluster_buf);
}

static int fat32_alloc_cluster(uint32_t *out_cluster) {
    if (!out_cluster) return 0;
    for (uint32_t c = 2; c <= g_max_cluster; ++c) {
        uint32_t value;
        if (!fat32_read_entry(c, &value)) return 0;
        if (value == 0) {
            if (!fat32_set_entry(c, 0x0FFFFFFFu)) return 0;
            if (!fat32_zero_cluster(c)) {
                fat32_set_entry(c, 0);
                return 0;
            }
            *out_cluster = c;
            return 1;
        }
    }
    return 0;
}

static int fat32_free_chain(uint32_t cluster) {
    uint32_t guard = 0;
    while (cluster >= 2 && cluster <= g_max_cluster && guard++ <= g_max_cluster) {
        uint32_t next;
        if (!fat32_read_entry(cluster, &next)) return 0;
        if (!fat32_set_entry(cluster, 0)) return 0;
        if (is_end_of_chain(next) || next < 2) return 1;
        cluster = next;
    }
    return 0;
}

static int make_component_name(const char *component, uint8_t out[11]) {
    if (!component || !component[0] || component[0] == '.' || component[0] == ' ') return 0;
    int dot = -1, len = 0;
    while (component[len]) {
        if (component[len] == '/') return 0;
        if (component[len] == '.') {
            if (dot >= 0) return 0;
            dot = len;
        }
        ++len;
        if (len > 12) return 0;
    }
    int base_len = dot >= 0 ? dot : len;
    int ext_len = dot >= 0 ? len - dot - 1 : 0;
    if (base_len < 1 || base_len > 8 || ext_len > 3) return 0;
    for (int i = 0; i < 11; ++i) out[i] = ' ';
    for (int i = 0; i < base_len; ++i) {
        char c = component[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out[i] = (uint8_t)c;
    }
    for (int i = 0; i < ext_len; ++i) {
        char c = component[dot + 1 + i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out[8 + i] = (uint8_t)c;
    }
    return 1;
}

/* Finds an entry and its exact directory cluster/slot. */
static int find_entry_location(uint32_t dir_cluster, const uint8_t name[11],
                               uint32_t *out_cluster, uint32_t *out_index,
                               fat32_dirent_t *out_entry) {
    uint32_t cluster = dir_cluster;
    while (!is_end_of_chain(cluster) && cluster >= 2 && cluster <= g_max_cluster) {
        if (!read_cluster(cluster)) return 0;
        uint32_t n = (g_sectors_per_cluster * g_bytes_per_sector) / sizeof(fat32_dirent_t);
        fat32_dirent_t *entries = (fat32_dirent_t *)g_cluster_buf;
        for (uint32_t i = 0; i < n; ++i) {
            uint8_t first = entries[i].name[0];
            if (first == 0x00) return 0;
            if (first == 0xE5 || entries[i].attr == ATTR_LFN || (entries[i].attr & ATTR_VOLUME_ID)) continue;
            if (memcmp_local(entries[i].name, name, 11) == 0) {
                if (out_cluster) *out_cluster = cluster;
                if (out_index) *out_index = i;
                if (out_entry) *out_entry = entries[i];
                return 1;
            }
        }
        uint32_t next = get_next_cluster(cluster);
        if (next < 2) return 0;
        cluster = next;
    }
    return 0;
}

/* Returns the parent directory cluster and final 8.3 component. */
static int resolve_parent(const char *path, uint32_t *out_parent, uint8_t out_name[11]) {
    if (!path || !path[0] || path[0] != '/') return 0;
    int len = 0;
    while (path[len]) ++len;
    while (len > 1 && path[len - 1] == '/') --len;
    int slash = len - 1;
    while (slash >= 0 && path[slash] != '/') --slash;
    if (slash < 0 || slash == len - 1) return 0;

    char parent[256];
    if (slash == 0) {
        parent[0] = '/'; parent[1] = 0;
    } else {
        if (slash >= (int)sizeof(parent)) return 0;
        for (int i = 0; i < slash; ++i) parent[i] = path[i];
        parent[slash] = 0;
    }
    char component[13];
    int clen = len - slash - 1;
    if (clen <= 0 || clen >= (int)sizeof(component)) return 0;
    for (int i = 0; i < clen; ++i) component[i] = path[slash + 1 + i];
    component[clen] = 0;

    fat32_dirent_t dir;
    if (!resolve_path(parent, &dir) || !(dir.attr & ATTR_DIRECTORY)) return 0;
    *out_parent = dirent_cluster(&dir);
    if (*out_parent == 0) *out_parent = g_root_cluster;
    return make_component_name(component, out_name);
}

static int write_dirent(uint32_t cluster, uint32_t index, const fat32_dirent_t *entry) {
    if (!read_cluster(cluster)) return 0;
    uint32_t n = (g_sectors_per_cluster * g_bytes_per_sector) / sizeof(fat32_dirent_t);
    if (index >= n) return 0;
    ((fat32_dirent_t *)g_cluster_buf)[index] = *entry;
    return ahci_write_sectors(cluster_to_lba(cluster), g_sectors_per_cluster, g_cluster_buf);
}

static int find_free_dir_slot(uint32_t start, uint32_t *out_cluster, uint32_t *out_index) {
    uint32_t cluster = start;
    uint32_t guard = 0;
    while (cluster >= 2 && cluster <= g_max_cluster && guard++ <= g_max_cluster) {
        if (!read_cluster(cluster)) return 0;
        uint32_t n = (g_sectors_per_cluster * g_bytes_per_sector) / sizeof(fat32_dirent_t);
        fat32_dirent_t *entries = (fat32_dirent_t *)g_cluster_buf;
        for (uint32_t i = 0; i < n; ++i) {
            if (entries[i].name[0] == 0x00 || entries[i].name[0] == 0xE5) {
                *out_cluster = cluster; *out_index = i; return 1;
            }
        }
        uint32_t next;
        if (!fat32_read_entry(cluster, &next)) return 0;
        if (is_end_of_chain(next)) {
            uint32_t fresh;
            if (!fat32_alloc_cluster(&fresh)) return 0;
            if (!fat32_set_entry(cluster, fresh)) { fat32_free_chain(fresh); return 0; }
            *out_cluster = fresh; *out_index = 0; return 1;
        }
        cluster = next;
    }
    return 0;
}

int fat32_mkdir(const char *path) {
    if (!g_mounted) return 0;
    uint32_t parent;
    uint8_t name[11];
    if (!resolve_parent(path, &parent, name)) return 0;
    fat32_dirent_t existing;
    if (find_entry(parent, name, &existing)) return 0;

    uint32_t dir_cluster;
    if (!fat32_alloc_cluster(&dir_cluster)) return 0;
    /* Initialize the standard . and .. entries before publishing the parent entry. */
    if (!read_cluster(dir_cluster)) { fat32_free_chain(dir_cluster); return 0; }
    fat32_dirent_t *entries = (fat32_dirent_t *)g_cluster_buf;
    memset(entries, 0, g_sectors_per_cluster * g_bytes_per_sector);
    for (int i = 0; i < 11; ++i) entries[0].name[i] = ' ';
    entries[0].name[0] = '.'; entries[0].attr = ATTR_DIRECTORY; set_dirent_cluster(&entries[0], dir_cluster);
    for (int i = 0; i < 11; ++i) entries[1].name[i] = ' ';
    entries[1].name[0] = '.'; entries[1].name[1] = '.'; entries[1].attr = ATTR_DIRECTORY; set_dirent_cluster(&entries[1], parent);
    if (!ahci_write_sectors(cluster_to_lba(dir_cluster), g_sectors_per_cluster, g_cluster_buf)) { fat32_free_chain(dir_cluster); return 0; }

    uint32_t slot_cluster, slot;
    if (!find_free_dir_slot(parent, &slot_cluster, &slot)) { fat32_free_chain(dir_cluster); return 0; }

    fat32_dirent_t e;
    memset(&e, 0, sizeof(e));
    memcpy(e.name, name, 11);
    e.attr = ATTR_DIRECTORY;
    set_dirent_cluster(&e, dir_cluster);
    if (!write_dirent(slot_cluster, slot, &e)) { fat32_free_chain(dir_cluster); return 0; }
    return 1;
}

int fat32_write_file(const char *path, const void *buf, uint32_t size) {
    if (!g_mounted || (!buf && size)) return 0;
    if (size > 16u * 1024u * 1024u) return 0; /* bounded installation primitive */

    uint32_t parent;
    uint8_t name[11];
    if (!resolve_parent(path, &parent, name)) return 0;

    fat32_dirent_t old;
    int exists = find_entry(parent, name, &old);
    if (exists && (old.attr & ATTR_DIRECTORY)) return 0;

    uint32_t first = 0, last = 0;
    uint32_t cluster_bytes = g_sectors_per_cluster * g_bytes_per_sector;
    uint32_t needed = size ? (size + cluster_bytes - 1) / cluster_bytes : 0;

    for (uint32_t i = 0; i < needed; ++i) {
        uint32_t fresh;
        if (!fat32_alloc_cluster(&fresh)) { if (first) fat32_free_chain(first);
            return 0; }
        if (!first) first = fresh;
        if (last && !fat32_set_entry(last, fresh)) { fat32_free_chain(fresh); if (first) fat32_free_chain(first);
            return 0; }
        last = fresh;
    }

    const uint8_t *src = (const uint8_t *)buf;
    uint32_t remaining = size;
    uint32_t cluster = first;
    while (remaining) {
        uint32_t chunk = remaining > cluster_bytes ? cluster_bytes : remaining;
        memset(g_cluster_buf, 0, cluster_bytes);
        memcpy(g_cluster_buf, src, chunk);
        if (!ahci_write_sectors(cluster_to_lba(cluster), g_sectors_per_cluster, g_cluster_buf)) {
            if (first) fat32_free_chain(first);
            return 0;
        }
        src += chunk; remaining -= chunk;
        if (!remaining) break;
        uint32_t next;
        if (!fat32_read_entry(cluster, &next) || is_end_of_chain(next)) {
            fat32_free_chain(first);
            return 0;
        }
        cluster = next;
    }

    uint32_t slot_cluster, slot;
    if (exists) {
        /* Reuse the existing entry's exact location. */
        if (!find_entry_location(parent, name, &slot_cluster, &slot, 0)) {
            if (first) fat32_free_chain(first);
            return 0;
        }
    } else if (!find_free_dir_slot(parent, &slot_cluster, &slot)) {
        if (first) fat32_free_chain(first);
            return 0;
    }

    fat32_dirent_t e;
    memset(&e, 0, sizeof(e));
    memcpy(e.name, name, 11);
    e.attr = 0x20; /* archive */
    set_dirent_cluster(&e, first);
    e.file_size = size;
    if (!write_dirent(slot_cluster, slot, &e)) {
        if (first) fat32_free_chain(first);
        return 0;
    }

    if (exists) {
        uint32_t old_cluster = dirent_cluster(&old);
        if (old_cluster >= 2) fat32_free_chain(old_cluster);
    }
    return 1;
}

/* Removes a regular file. The directory entry is tombstoned before the
 * cluster chain is released so a failed later write cannot expose stale
 * metadata as a live file. */
int fat32_remove_file(const char *path) {
    if (!g_mounted || !path) return 0;

    uint32_t parent;
    uint8_t name[11];
    if (!resolve_parent(path, &parent, name)) return 0;

    uint32_t slot_cluster, slot;
    fat32_dirent_t entry;
    if (!find_entry_location(parent, name, &slot_cluster, &slot, &entry)) return 0;
    if (entry.attr & ATTR_DIRECTORY) return 0;

    if (!read_cluster(slot_cluster)) return 0;
    fat32_dirent_t *entries = (fat32_dirent_t *)g_cluster_buf;
    entries[slot].name[0] = 0xE5;
    if (!ahci_write_sectors(cluster_to_lba(slot_cluster), g_sectors_per_cluster, g_cluster_buf)) return 0;

    uint32_t first = dirent_cluster(&entry);
    if (first >= 2 && !fat32_free_chain(first)) return 0;
    return 1;
}

/* Removes a directory only when it contains no user entries. This keeps the
 * installer rollback primitive deliberately conservative: no recursive
 * deletion is performed by the package manager. */
int fat32_remove_empty_dir(const char *path) {
    if (!g_mounted || !path || (path[0] == '/' && path[1] == 0)) return 0;

    uint32_t parent;
    uint8_t name[11];
    if (!resolve_parent(path, &parent, name)) return 0;

    uint32_t slot_cluster, slot;
    fat32_dirent_t entry;
    if (!find_entry_location(parent, name, &slot_cluster, &slot, &entry)) return 0;
    if (!(entry.attr & ATTR_DIRECTORY)) return 0;

    uint32_t cluster = dirent_cluster(&entry);
    uint32_t guard = 0;
    while (cluster >= 2 && cluster <= g_max_cluster && guard++ <= g_max_cluster) {
        if (!read_cluster(cluster)) return 0;
        uint32_t n = (g_sectors_per_cluster * g_bytes_per_sector) / sizeof(fat32_dirent_t);
        fat32_dirent_t *entries = (fat32_dirent_t *)g_cluster_buf;
        for (uint32_t i = 0; i < n; ++i) {
            uint8_t first = entries[i].name[0];
            if (first == 0x00) break;
            if (first == 0xE5 || entries[i].attr == ATTR_LFN || (entries[i].attr & ATTR_VOLUME_ID)) continue;
            if (entries[i].name[0] == '.' &&
                (entries[i].name[1] == ' ' || (entries[i].name[1] == '.' && entries[i].name[2] == ' '))) continue;
            return 0; /* non-special live entry */
        }
        uint32_t next;
        if (!fat32_read_entry(cluster, &next)) return 0;
        if (is_end_of_chain(next)) break;
        cluster = next;
    }

    if (!read_cluster(slot_cluster)) return 0;
    ((fat32_dirent_t *)g_cluster_buf)[slot].name[0] = 0xE5;
    if (!ahci_write_sectors(cluster_to_lba(slot_cluster), g_sectors_per_cluster, g_cluster_buf)) return 0;

    return fat32_free_chain(dirent_cluster(&entry));
}
