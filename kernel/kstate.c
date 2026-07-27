#include "kstate.h"

static nexus_boot_info_t *g_boot_info;

void kstate_set_boot_info(nexus_boot_info_t *bi) {
    g_boot_info = bi;
}

nexus_boot_info_t *kstate_get_boot_info(void) {
    return g_boot_info;
}

void kstate_mem_summary(uint64_t *out_total_pages, uint64_t *out_conventional_pages) {
    uint64_t total = 0, conventional = 0;

    if (g_boot_info != 0 && g_boot_info->mmap.descriptor_size != 0) {
        uint8_t *base = (uint8_t *)(uintptr_t)g_boot_info->mmap.map_base;
        uint64_t stride = g_boot_info->mmap.descriptor_size;
        uint64_t count = g_boot_info->mmap.map_size / stride;

        for (uint64_t i = 0; i < count; i++) {
            nexus_efi_mmap_entry_t *e = (nexus_efi_mmap_entry_t *)(base + i * stride);

            /* MMIO, PAL code и "unusable" — не физическая RAM, не считаем
             * их в "общей обнаруженной памяти", иначе цифра будет врать. */
            int is_ram = (e->type != NEXUS_MEM_RESERVED &&
                          e->type != NEXUS_MEM_UNUSABLE &&
                          e->type != NEXUS_MEM_MMIO &&
                          e->type != NEXUS_MEM_MMIO_PORT_SPACE &&
                          e->type != NEXUS_MEM_PAL_CODE);

            if (is_ram) total += e->number_of_pages;
            if (e->type == NEXUS_MEM_CONVENTIONAL) {
                conventional += e->number_of_pages;
            }
        }
    }

    if (out_total_pages) *out_total_pages = total;
    if (out_conventional_pages) *out_conventional_pages = conventional;
}
