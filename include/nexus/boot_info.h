/* NexusOS: контракт между бутлоадером и ядром. Меняешь тут — меняй в обеих
 * сторонах одновременно (bootloader пишет, kernel читает). */
#ifndef NEXUSOS_BOOT_INFO_H
#define NEXUSOS_BOOT_INFO_H

#include <stdint.h>

#define NEXUS_PIXFMT_RGB   0
#define NEXUS_PIXFMT_BGR   1
#define NEXUS_PIXFMT_OTHER 2

typedef struct {
    uint64_t base;
    uint64_t size;              /* байт */
    uint32_t width;
    uint32_t height;
    uint32_t pixels_per_scanline;
    uint32_t pixel_format;      /* NEXUS_PIXFMT_* */
} nexus_framebuffer_t;

typedef struct {
    uint64_t map_base;          /* массив EFI_MEMORY_DESCRIPTOR */
    uint64_t map_size;          /* байт всего */
    uint64_t descriptor_size;   /* байт на один дескриптор (НЕ sizeof!) */
    uint32_t descriptor_version;
} nexus_memory_map_t;

/* Раскладка ОДНОЙ записи EFI memory map. ВАЖНО: при переборе массива шаг
 * между записями — nexus_memory_map_t.descriptor_size, а НЕ
 * sizeof(nexus_efi_mmap_entry_t) — прошивка может использовать более
 * широкий дескриптор про запас на будущее (так требует спецификация UEFI). */
typedef struct {
    uint32_t type;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} nexus_efi_mmap_entry_t;

/* Подмножество значений EFI_MEMORY_TYPE, которое нам реально нужно читать. */
#define NEXUS_MEM_RESERVED             0
#define NEXUS_MEM_LOADER_CODE          1
#define NEXUS_MEM_LOADER_DATA          2
#define NEXUS_MEM_BOOT_SERVICES_CODE   3
#define NEXUS_MEM_BOOT_SERVICES_DATA   4
#define NEXUS_MEM_RUNTIME_SERVICES_CODE 5
#define NEXUS_MEM_RUNTIME_SERVICES_DATA 6
#define NEXUS_MEM_CONVENTIONAL         7
#define NEXUS_MEM_UNUSABLE             8
#define NEXUS_MEM_ACPI_RECLAIM         9
#define NEXUS_MEM_ACPI_NVS             10
#define NEXUS_MEM_MMIO                 11
#define NEXUS_MEM_MMIO_PORT_SPACE      12
#define NEXUS_MEM_PAL_CODE             13

typedef struct {
    uint64_t magic;              /* для проверки, что структура валидна */
    nexus_framebuffer_t fb;
    nexus_memory_map_t mmap;
} nexus_boot_info_t;

#define NEXUS_BOOT_MAGIC 0x4E4558555342494EULL /* "NEXUSBIN" */

#endif
