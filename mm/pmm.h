#ifndef NEXUS_MM_PMM_H
#define NEXUS_MM_PMM_H

#include <nexus/types.h>
#include <nexus/multiboot.h>

#define PMM_PAGE_SIZE 4096

/* Инициализирует bitmap-аллокатор по карте памяти, полученной от GRUB */
void pmm_init(multiboot_info_t *mbi, u32 kernel_end_addr);

/* Выделяет одну физическую страницу (4 KiB), возвращает 0 если память кончилась */
u32 pmm_alloc_page(void);

/* Освобождает ранее выделенную страницу */
void pmm_free_page(u32 addr);

/* Статистика — для kprintf-диагностики при старте */
u32 pmm_total_pages(void);
u32 pmm_used_pages(void);

#endif /* NEXUS_MM_PMM_H */
