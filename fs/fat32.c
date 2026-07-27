/* NexusOS: FAT32 read-only. См. предупреждения и упрощения в fat32.h. */
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
    g_fat_start_lba = partition_lba + bpb->reserved_sectors;
    g_cluster_heap_start_lba = g_fat_start_lba + (uint64_t)bpb->num_fats * bpb->fat_size32;

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
