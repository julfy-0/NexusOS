#ifndef NEXUSOS_VMM_H
#define NEXUSOS_VMM_H

#include <stdint.h>

/* Virtual Memory Manager — 4 KiB page mapping API.
 *
 * The kernel now executes from its higher-half virtual alias while the
 * low identity map is intentionally retained for early physical/MMIO access.
 * VMM adds a safe API for
 * creating/removing individual 4 KiB mappings and translating virtual
 * addresses. Page-table pages are obtained from the PMM.
 */
void vmm_init(void);

int vmm_map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags);
int vmm_unmap_page(uint64_t virtual_address);
uint64_t vmm_virt_to_phys(uint64_t virtual_address);
int vmm_is_mapped(uint64_t virtual_address);

uint64_t vmm_page_size(void);
uint64_t vmm_mapped_pages(void);

#define VMM_PAGE_PRESENT  0x001ULL
#define VMM_PAGE_WRITABLE 0x002ULL
#define VMM_PAGE_USER     0x004ULL
#define VMM_PAGE_PWT      0x008ULL
#define VMM_PAGE_PCD      0x010ULL
#define VMM_PAGE_NX       (1ULL << 63)

#endif
