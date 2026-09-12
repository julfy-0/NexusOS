#include <stdint.h>
#include "vmm.h"
#include "paging.h"
#include "pmm.h"

#define VMM_PAGE_SIZE 4096ULL

static int g_ready;
static uint64_t g_mapped_pages;

void vmm_init(void) {
    g_ready = 1;
    g_mapped_pages = 0;
}

int vmm_map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags) {
    if (!g_ready) return 0;
    if ((virtual_address & (VMM_PAGE_SIZE - 1)) != 0) return 0;
    if ((physical_address & (VMM_PAGE_SIZE - 1)) != 0) return 0;
    if (paging_map_page(virtual_address, physical_address, flags) != 0) return 0;
    g_mapped_pages++;
    return 1;
}

int vmm_unmap_page(uint64_t virtual_address) {
    if (!g_ready || (virtual_address & (VMM_PAGE_SIZE - 1)) != 0) return 0;
    int result = paging_unmap_page(virtual_address);
    if (result == 1 && g_mapped_pages != 0) g_mapped_pages--;
    return result;
}

uint64_t vmm_virt_to_phys(uint64_t virtual_address) {
    if (!g_ready) return 0;
    return paging_virt_to_phys(virtual_address);
}

int vmm_is_mapped(uint64_t virtual_address) {
    if (!g_ready) return 0;
    return paging_virt_to_phys(virtual_address) != 0;
}

uint64_t vmm_page_size(void) { return VMM_PAGE_SIZE; }
uint64_t vmm_mapped_pages(void) { return g_mapped_pages; }
