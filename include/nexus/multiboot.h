#ifndef NEXUS_MULTIBOOT_H
#define NEXUS_MULTIBOOT_H

#include <nexus/types.h>

/*
 * Урезанная структура multiboot_info — только поля, которые нужны
 * NexusOS сейчас (флаги + карта памяти). Полная спецификация:
 * https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
 */
typedef struct NX_PACKED {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
} multiboot_info_t;

typedef struct NX_PACKED {
    u32 size;
    u64 addr;
    u64 len;
    u32 type;
} multiboot_mmap_entry_t;

#define MULTIBOOT_MMAP_AVAILABLE 1

#endif /* NEXUS_MULTIBOOT_H */
