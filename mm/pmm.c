/*
 * NexusOS — простейший физический менеджер памяти на основе bitmap.
 * Каждый бит соответствует одной странице 4 KiB: 1 = занята, 0 = свободна.
 * Виртуальная память (paging) сюда не входит — это следующий шаг,
 * для которого этот модуль уже готовит физический фундамент.
 */
#include "pmm.h"
#include <lib/string.h>
#include <lib/stdio.h>

/* Поддерживаем до 4 GiB физической памяти -> 1 048 576 страниц ->
 * 131 072 байта на bitmap. Для hobby-OS этого достаточно с запасом. */
#define PMM_MAX_PAGES (1024u * 1024u)
#define PMM_BITMAP_SIZE (PMM_MAX_PAGES / 8u)

static u8  bitmap[PMM_BITMAP_SIZE];
static u32 total_pages = 0;
static u32 used_pages  = 0;

static inline void bitmap_set(u32 bit)   { bitmap[bit / 8] |= (u8)(1 << (bit % 8)); }
static inline void bitmap_clear(u32 bit) { bitmap[bit / 8] &= (u8)~(1 << (bit % 8)); }
static inline int  bitmap_test(u32 bit)  { return bitmap[bit / 8] & (1 << (bit % 8)); }

void pmm_init(multiboot_info_t *mbi, u32 kernel_end_addr) {
    memset(bitmap, 0xFF, sizeof(bitmap)); /* по умолчанию всё "занято" */
    total_pages = 0;

    if (!(mbi->flags & (1 << 6))) {
        kprintf("[PMM] Внимание: GRUB не передал карту памяти (mmap)\n");
        return;
    }

    multiboot_mmap_entry_t *entry = (multiboot_mmap_entry_t *)mbi->mmap_addr;
    u32 mmap_end = mbi->mmap_addr + mbi->mmap_length;

    while ((u32)entry < mmap_end) {
        if (entry->type == MULTIBOOT_MMAP_AVAILABLE) {
            u64 start = entry->addr;
            u64 end   = entry->addr + entry->len;

            for (u64 addr = start; addr + PMM_PAGE_SIZE <= end; addr += PMM_PAGE_SIZE) {
                u32 page = (u32)(addr / PMM_PAGE_SIZE);
                if (page < PMM_MAX_PAGES) {
                    bitmap_clear(page);
                    total_pages++;
                }
            }
        }
        entry = (multiboot_mmap_entry_t *)((u32)entry + entry->size + sizeof(entry->size));
    }

    /* Резервируем область, где физически лежит само ядро — её нельзя
     * случайно раздать под что-то другое. */
    u32 kernel_pages = (kernel_end_addr / PMM_PAGE_SIZE) + 1;
    for (u32 page = 0; page < kernel_pages; page++) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            used_pages++;
        }
    }

    kprintf("[PMM] Доступно страниц: %u (~%u MiB)\n",
            total_pages, (total_pages * PMM_PAGE_SIZE) / (1024 * 1024));
}

u32 pmm_alloc_page(void) {
    for (u32 page = 0; page < total_pages; page++) {
        if (!bitmap_test(page)) {
            bitmap_set(page);
            used_pages++;
            return page * PMM_PAGE_SIZE;
        }
    }
    return 0; /* память кончилась */
}

void pmm_free_page(u32 addr) {
    u32 page = addr / PMM_PAGE_SIZE;
    if (bitmap_test(page)) {
        bitmap_clear(page);
        used_pages--;
    }
}

u32 pmm_total_pages(void) { return total_pages; }
u32 pmm_used_pages(void)  { return used_pages; }
