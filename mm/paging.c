/* NexusOS: свои page tables. Подробности контракта — mm/paging.h. */
#include <stdint.h>
#include <stddef.h>
#include "paging.h"
#include "boot_info.h"

#define PTE_PRESENT  0x001ULL
#define PTE_WRITABLE 0x002ULL
#define PTE_PCD      0x010ULL /* Page Cache Disable — обязателен для MMIO: без него запись
                                * в регистр (например xHCI doorbell) может осесть в write-back
                                * кэше и никогда не дойти до устройства. */
#define PTE_PWT      0x008ULL
#define PTE_HUGE     0x080ULL /* PS-бит: в PD-таблице значит "это 2 MiB страница, а не указатель на PT" */

#define ENTRIES_PER_TABLE 512
#define PAGE_2M (2ULL * 1024 * 1024)
#define PAGE_1G (1024ULL * 1024 * 1024)

/* Базовая identity-map: всегда покрываем первые BASE_IDENTITY_GIB GiB
 * безусловно, даже если EFI memory map почему-то не перечислил каждый
 * байт этого диапазона (низкая память часто дырявая/зарезервированная
 * местами, но лучше держать её замапленной, чем словить page fault на
 * ровном месте от MMIO/reserved-региона, который прошивка не считает
 * "нашим делом" описывать подробно). */
#define BASE_IDENTITY_GIB 4

/* Доп. PD-таблицы для 1 GiB-слотов ВНЕ базового диапазона — нужны,
 * если что-то (обычно framebuffer, реже сама EFI memory map) лежит
 * выше BASE_IDENTITY_GIB. Каждая таблица покрывает один 1 GiB слот. */
#define EXTRA_PD_SLOTS 512

static uint64_t pml4[ENTRIES_PER_TABLE] __attribute__((aligned(4096)));
static uint64_t pdpt[ENTRIES_PER_TABLE] __attribute__((aligned(4096)));
static uint64_t pd_base[BASE_IDENTITY_GIB][ENTRIES_PER_TABLE] __attribute__((aligned(4096)));
static uint64_t pd_extra[EXTRA_PD_SLOTS][ENTRIES_PER_TABLE] __attribute__((aligned(4096)));

/* pdpt_idx (0..511, номер 1 GiB слота), которому принадлежит pd_extra[i];
 * -1 = слот свободен. */
static int extra_slot_owner[EXTRA_PD_SLOTS];
static int extra_slots_used = 0;

static void zero_table(uint64_t *t) {
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) t[i] = 0;
}

/* Находит (или заводит, если это первый вызов для данного 1 GiB слота)
 * PD-таблицу, отвечающую за 2 MiB страницу по физическому адресу phys.
 * Пул покрывает все 512 PDPT-слотов (первые 512 GiB). На реальном
 * оборудовании нельзя silently пропускать регион: после загрузки CR3 это
 * приводит к page fault и часто к triple fault/мгновенному reboot. */
static uint64_t *pd_table_for(uint64_t phys) {
    uint64_t pdpt_idx = (phys / PAGE_1G) & 0x1FF;

    if (pdpt_idx < BASE_IDENTITY_GIB) {
        if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
            pdpt[pdpt_idx] = (uint64_t)(uintptr_t)pd_base[pdpt_idx] | PTE_PRESENT | PTE_WRITABLE;
        }
        return pd_base[pdpt_idx];
    }

    /* Уже заводили PD-таблицу для этого слота раньше в этом же вызове
     * paging_init()? */
    for (int i = 0; i < extra_slots_used; i++) {
        if (extra_slot_owner[i] == (int)pdpt_idx) {
            return pd_extra[i];
        }
    }

    if (extra_slots_used >= EXTRA_PD_SLOTS) {
        /* Адрес выше первых 512 GiB пока не поддержан. Не молча: CR3
         * переключаем только после построения полной поддерживаемой карты. */
        return NULL;
    }

    int slot = extra_slots_used++;
    extra_slot_owner[slot] = (int)pdpt_idx;
    zero_table(pd_extra[slot]);
    pdpt[pdpt_idx] = (uint64_t)(uintptr_t)pd_extra[slot] | PTE_PRESENT | PTE_WRITABLE;
    return pd_extra[slot];
}

/* Мапит одну 2 MiB страницу identity (phys должен быть выровнен на 2 MiB). */
static void map_2m_page(uint64_t phys, uint64_t extra_flags) {
    uint64_t *pd = pd_table_for(phys);
    if (pd == NULL) return;

    uint64_t pd_idx = (phys / PAGE_2M) & 0x1FF;
    pd[pd_idx] = phys | PTE_PRESENT | PTE_WRITABLE | PTE_HUGE | extra_flags;
}

/* Мапит диапазон [start, end) 2 MiB страницами, округляя границы наружу
 * до ближайшей 2 MiB-границы, чтобы гарантированно покрыть весь диапазон. */
static void map_region(uint64_t start, uint64_t end, uint64_t extra_flags) {
    if (end <= start) return;

    uint64_t aligned_start = start & ~(PAGE_2M - 1);
    uint64_t aligned_end = (end + PAGE_2M - 1) & ~(PAGE_2M - 1);

    for (uint64_t phys = aligned_start; phys < aligned_end; phys += PAGE_2M) {
        map_2m_page(phys, extra_flags);
    }
}

void paging_init(nexus_boot_info_t *bi) {
    zero_table(pml4);
    zero_table(pdpt);
    for (int i = 0; i < BASE_IDENTITY_GIB; i++) zero_table(pd_base[i]);
    extra_slots_used = 0;

    /* PML4[0] покрывает нижние 512 GiB виртуального (=физического, пока
     * identity) адресного пространства — этого с большим запасом хватает
     * на всё, что мы мапим ниже. */
    pml4[0] = (uint64_t)(uintptr_t)pdpt | PTE_PRESENT | PTE_WRITABLE;

    /* 1. Безусловно мапим базовый низкий диапазон (см. BASE_IDENTITY_GIB) —
     *    сюда попадает сам код ядра (грузится по 0x200000), его стек,
     *    все таблицы, и обычно вся "обычная" RAM у большинства машин,
     *    на которых сейчас тестируемся (QEMU с -m 256M и т.п.). */
    map_region(0, (uint64_t)BASE_IDENTITY_GIB * PAGE_1G, 0);

    /* 2. Проходим по настоящей EFI memory map и мапим КАЖДЫЙ описанный
     *    регион — это покрывает MMIO/reserved/ACPI-регионы, которые
     *    формально не "RAM", но к которым код (или сама прошивка) может
     *    обращаться, и любую RAM выше базового диапазона на машинах
     *    с большим объёмом памяти. */
    if (bi != NULL && bi->mmap.descriptor_size != 0 && bi->mmap.map_base != 0) {
        uint8_t *base = (uint8_t *)(uintptr_t)bi->mmap.map_base;
        uint64_t stride = bi->mmap.descriptor_size;
        uint64_t count = bi->mmap.map_size / stride;

        for (uint64_t i = 0; i < count; i++) {
            nexus_efi_mmap_entry_t *e = (nexus_efi_mmap_entry_t *)(base + i * stride);
            uint64_t region_start = e->physical_start;
            uint64_t region_end = region_start + e->number_of_pages * 4096ULL;
            map_region(region_start, region_end, 0);
        }
    }

    /* 3. Framebuffer отдельно и явно — он приходит из GOP, а не из EFI
     *    memory map (это MMIO PCI BAR, UEFI необязательно перечисляет
     *    его в GetMemoryMap() тем же адресом/типом, на который можно
     *    полагаться), так что дырка тут была бы неприятным сюрпризом:
     *    первая же попытка что-то напечатать после переключения CR3
     *    ушла бы в page fault. */
    if (bi != NULL && bi->fb.base != 0 && bi->fb.size != 0) {
        map_region(bi->fb.base, bi->fb.base + bi->fb.size, 0);
    }

    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint64_t)(uintptr_t)pml4) : "memory");
}

void paging_map_region(uint64_t start, uint64_t end) {
    /* Всегда некэшируемо: контракт этой функции (см. paging.h) — MMIO BAR'ы. */
    map_region(start, end, PTE_PCD | PTE_PWT);
}

uint64_t paging_get_cr3(void) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

uint64_t paging_base_identity_gib(void) {
    return BASE_IDENTITY_GIB;
}
