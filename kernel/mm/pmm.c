/* NexusOS physical memory manager — first real PMM milestone.
 *
 * The UEFI memory map is the source of truth. We start with every tracked
 * page marked used, release only EfiConventionalMemory, then reserve regions
 * occupied by the kernel, boot information, memory map, framebuffer and low
 * boot memory. This keeps the allocator conservative and avoids handing out
 * firmware/MMIO memory as RAM.
 */
#include <stdint.h>
#include <stddef.h>
#include "pmm.h"
#include "boot_info.h"

#define PAGE_SIZE 4096ULL
#define PMM_MAX_PHYS (64ULL * 1024ULL * 1024ULL * 1024ULL)
#define PMM_MAX_PAGES (PMM_MAX_PHYS / PAGE_SIZE)
#define PMM_BITMAP_BYTES ((PMM_MAX_PAGES + 7ULL) / 8ULL)

static uint8_t g_bitmap[PMM_BITMAP_BYTES] __attribute__((aligned(4096)));
static uint64_t g_total_pages;
static uint64_t g_free_pages;
static uint64_t g_max_physical_address;
static int g_ready;

extern char __kernel_start;
extern char __kernel_end;

static void bitmap_set(uint64_t page) {
    g_bitmap[page >> 3] |= (uint8_t)(1U << (page & 7));
}

static void bitmap_clear(uint64_t page) {
    g_bitmap[page >> 3] &= (uint8_t)~(1U << (page & 7));
}

static int bitmap_test(uint64_t page) {
    return (g_bitmap[page >> 3] & (uint8_t)(1U << (page & 7))) != 0;
}

static uint64_t align_down(uint64_t value) {
    return value & ~(PAGE_SIZE - 1ULL);
}

static uint64_t align_up(uint64_t value) {
    if (value > UINT64_MAX - (PAGE_SIZE - 1ULL)) return UINT64_MAX & ~(PAGE_SIZE - 1ULL);
    return (value + PAGE_SIZE - 1ULL) & ~(PAGE_SIZE - 1ULL);
}

static void reserve_range(uint64_t start, uint64_t end) {
    if (end <= start || g_max_physical_address == 0) return;

    start = align_down(start);
    end = align_up(end);
    if (start >= g_max_physical_address) return;
    if (end > g_max_physical_address) end = g_max_physical_address;

    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
        uint64_t page = addr / PAGE_SIZE;
        if (page >= PMM_MAX_PAGES) break;
        if (!bitmap_test(page)) {
            bitmap_set(page);
            if (g_free_pages != 0) g_free_pages--;
        }
    }
}

static void release_range(uint64_t start, uint64_t end) {
    if (end <= start || g_max_physical_address == 0) return;

    start = align_up(start);
    end = align_down(end);
    if (start >= g_max_physical_address) return;
    if (end > g_max_physical_address) end = g_max_physical_address;

    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
        uint64_t page = addr / PAGE_SIZE;
        if (page >= PMM_MAX_PAGES) break;
        if (bitmap_test(page)) {
            bitmap_clear(page);
            g_free_pages++;
        }
    }
}

static void consider_max(const nexus_efi_mmap_entry_t *e) {
    if (e == NULL || e->number_of_pages == 0) return;
    if (e->physical_start >= PMM_MAX_PHYS) return;

    uint64_t bytes = e->number_of_pages * PAGE_SIZE;
    if (e->number_of_pages > UINT64_MAX / PAGE_SIZE) bytes = UINT64_MAX;
    uint64_t end = e->physical_start + bytes;
    if (end < e->physical_start || end > PMM_MAX_PHYS) end = PMM_MAX_PHYS;
    if (end > g_max_physical_address) g_max_physical_address = end;
}

void pmm_init(const void *boot_info_ptr) {
    const nexus_boot_info_t *bi = (const nexus_boot_info_t *)boot_info_ptr;

    g_ready = 0;
    g_total_pages = 0;
    g_free_pages = 0;
    g_max_physical_address = 0;

    for (uint64_t i = 0; i < PMM_BITMAP_BYTES; i++) g_bitmap[i] = 0xFF;

    if (bi == NULL || bi->mmap.map_base == 0 || bi->mmap.map_size == 0 ||
        bi->mmap.descriptor_size < sizeof(nexus_efi_mmap_entry_t)) {
        return;
    }

    const uint8_t *base = (const uint8_t *)(uintptr_t)bi->mmap.map_base;
    uint64_t stride = bi->mmap.descriptor_size;
    uint64_t count = bi->mmap.map_size / stride;

    /* First pass: determine the physical range we can represent. */
    for (uint64_t i = 0; i < count; i++) {
        const nexus_efi_mmap_entry_t *e =
            (const nexus_efi_mmap_entry_t *)(base + i * stride);
        consider_max(e);
    }

    g_total_pages = g_max_physical_address / PAGE_SIZE;
    if (g_total_pages > PMM_MAX_PAGES) g_total_pages = PMM_MAX_PAGES;

    /* Only conventional RAM becomes allocatable. Everything else remains used. */
    for (uint64_t i = 0; i < count; i++) {
        const nexus_efi_mmap_entry_t *e =
            (const nexus_efi_mmap_entry_t *)(base + i * stride);
        if (e->type != NEXUS_MEM_CONVENTIONAL) continue;

        uint64_t bytes = e->number_of_pages * PAGE_SIZE;
        if (e->number_of_pages > UINT64_MAX / PAGE_SIZE) bytes = UINT64_MAX;
        uint64_t end = e->physical_start + bytes;
        if (end < e->physical_start) end = UINT64_MAX;
        release_range(e->physical_start, end);
    }

    /* Never allocate the low 2 MiB: this contains legacy/firmware structures
     * and keeps the first page permanently unavailable. */
    reserve_range(0, 0x200000ULL);

    /* The linker symbols cover .text/.rodata/.data/.bss, including this PMM
     * bitmap and the page tables. */
    reserve_range((uint64_t)(uintptr_t)&__kernel_start,
                  (uint64_t)(uintptr_t)&__kernel_end);

    /* Boot info itself is a firmware-owned static object. The memory map was
     * allocated from EfiLoaderData, so neither may be recycled by PMM. */
    reserve_range((uint64_t)(uintptr_t)bi,
                  (uint64_t)(uintptr_t)bi + sizeof(*bi));
    reserve_range(bi->mmap.map_base, bi->mmap.map_base + bi->mmap.map_size);

    if (bi->fb.base != 0 && bi->fb.size != 0) {
        reserve_range(bi->fb.base, bi->fb.base + bi->fb.size);
    }

    /* Keep accounting exact for the represented bitmap range. */
    g_free_pages = 0;
    for (uint64_t page = 0; page < g_total_pages; page++) {
        if (!bitmap_test(page)) g_free_pages++;
    }

    g_ready = 1;
}

uint64_t pmm_alloc_page(void) {
    if (!g_ready) return 0;

    for (uint64_t page = 0; page < g_total_pages; page++) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            if (g_free_pages != 0) g_free_pages--;
            return page * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free_page(uint64_t physical_address) {
    if (!g_ready || (physical_address & (PAGE_SIZE - 1ULL)) != 0) return;

    uint64_t page = physical_address / PAGE_SIZE;
    if (page >= g_total_pages || page >= PMM_MAX_PAGES) return;

    if (bitmap_test(page)) {
        bitmap_clear(page);
        g_free_pages++;
    }
}

uint64_t pmm_total_pages(void) { return g_total_pages; }
uint64_t pmm_free_pages(void) { return g_free_pages; }
uint64_t pmm_used_pages(void) { return g_total_pages - g_free_pages; }
uint64_t pmm_max_physical_address(void) { return g_max_physical_address; }
uint64_t pmm_bitmap_bytes(void) { return PMM_BITMAP_BYTES; }

int pmm_is_page_free(uint64_t physical_address) {
    if (!g_ready || (physical_address & (PAGE_SIZE - 1ULL)) != 0) return 0;
    uint64_t page = physical_address / PAGE_SIZE;
    if (page >= g_total_pages || page >= PMM_MAX_PAGES) return 0;
    return !bitmap_test(page);
}
