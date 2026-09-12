#ifndef NEXUSOS_PMM_H
#define NEXUSOS_PMM_H

#include <stdint.h>

/* Physical page manager. Pages are 4 KiB and are tracked with a bitmap.
 * The first implementation consumes EFI EfiConventionalMemory regions and
 * reserves all boot/kernel-owned memory before exposing pages to callers. */
void pmm_init(const void *boot_info);

uint64_t pmm_alloc_page(void);
void pmm_free_page(uint64_t physical_address);

uint64_t pmm_total_pages(void);
uint64_t pmm_free_pages(void);
uint64_t pmm_used_pages(void);
uint64_t pmm_max_physical_address(void);
uint64_t pmm_bitmap_bytes(void);
int pmm_is_page_free(uint64_t physical_address);

#endif
