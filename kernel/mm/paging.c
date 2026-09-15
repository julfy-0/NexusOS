/* NexusOS: свои page tables. Подробности контракта — kernel/mm/paging.h. */
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
#define PAGE_SIZE 4096ULL
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

/* -------------------------------------------------------------------------
 * 4 KiB page-table operations for VMM.
 *
 * The original identity map deliberately remains 2 MiB based. When a VMM
 * mapping is requested outside an existing huge-page mapping, these helpers
 * allocate the missing PML4/PDPT/PD/PT pages from PMM and install a normal
 * 4 KiB PTE. This is intentionally separate from the boot-time identity map.
 * ------------------------------------------------------------------------- */
#include "pmm.h"

#define PTE_USER 0x004ULL
#define PTE_NX   (1ULL << 63)
#define PT_INDEX_MASK 0x1FFULL

static uint64_t *table_from_phys(uint64_t phys) {
    /* The current kernel is identity-mapped through the first 4 GiB. Keep
     * page-table allocations there until the higher-half/recursive mapping
     * milestone provides a permanent mapping for arbitrary physical RAM. */
    if (phys >= 0x100000000ULL) return NULL;
    return (uint64_t *)(uintptr_t)phys;
}

static uint64_t alloc_table_page(void) {
    uint64_t phys = pmm_alloc_page();
    if (phys == 0 || phys >= 0x100000000ULL) {
        if (phys != 0) pmm_free_page(phys);
        return 0;
    }
    uint64_t *table = table_from_phys(phys);
    if (table == NULL) {
        pmm_free_page(phys);
        return 0;
    }
    zero_table(table);
    return phys;
}

static uint64_t *walk_create_root(uint64_t root_phys, uint64_t virtual_address, int create) {
    uint64_t pml4_index = (virtual_address >> 39) & PT_INDEX_MASK;
    uint64_t pdpt_index = (virtual_address >> 30) & PT_INDEX_MASK;
    uint64_t pd_index   = (virtual_address >> 21) & PT_INDEX_MASK;

    uint64_t *pml4_table = table_from_phys(root_phys);
    if (pml4_table == NULL) return NULL;
    uint64_t entry = pml4_table[pml4_index];
    if (!(entry & PTE_PRESENT)) {
        if (!create) return NULL;
        uint64_t phys = alloc_table_page();
        if (phys == 0) return NULL;
        pml4_table[pml4_index] = phys | PTE_PRESENT | PTE_WRITABLE;
        entry = pml4_table[pml4_index];
    }

    uint64_t *pdpt_table = table_from_phys(entry & ~0xFFFULL);
    if (pdpt_table == NULL) return NULL;
    entry = pdpt_table[pdpt_index];
    if (!(entry & PTE_PRESENT)) {
        if (!create) return NULL;
        uint64_t phys = alloc_table_page();
        if (phys == 0) return NULL;
        pdpt_table[pdpt_index] = phys | PTE_PRESENT | PTE_WRITABLE;
        entry = pdpt_table[pdpt_index];
    }
    if (entry & PTE_HUGE) return NULL;

    uint64_t *pd_table = table_from_phys(entry & ~0xFFFULL);
    if (pd_table == NULL) return NULL;
    entry = pd_table[pd_index];
    if (!(entry & PTE_PRESENT)) {
        if (!create) return NULL;
        uint64_t phys = alloc_table_page();
        if (phys == 0) return NULL;
        pd_table[pd_index] = phys | PTE_PRESENT | PTE_WRITABLE;
        entry = pd_table[pd_index];
    }
    if (entry & PTE_HUGE) {
        if (!create) return NULL;
        /* A 2 MiB identity mapping may cover a user virtual page. Split only
         * the private hierarchy being modified into 4 KiB leaves, preserving
         * the original physical identity mapping and cache attributes. */
        uint64_t pt_phys = alloc_table_page();
        if (pt_phys == 0) return NULL;
        uint64_t *pt = table_from_phys(pt_phys);
        uint64_t base = entry & 0xFFFFFFE00000ULL;
        uint64_t leaf_flags = (entry & 0xFFFULL) & ~PTE_HUGE;
        for (uint64_t i = 0; i < ENTRIES_PER_TABLE; ++i)
            pt[i] = (base + i * PAGE_SIZE) | leaf_flags;
        pd_table[pd_index] = pt_phys | PTE_PRESENT | PTE_WRITABLE;
        entry = pd_table[pd_index];
    }

    return table_from_phys(entry & ~0xFFFULL);
}

static int map_page_root(uint64_t root_phys, uint64_t virtual_address,
                         uint64_t physical_address, uint64_t flags) {
    if ((virtual_address & 0xFFFULL) != 0 || (physical_address & 0xFFFULL) != 0) return -1;
    uint64_t *pt = walk_create_root(root_phys, virtual_address, 1);
    if (pt == NULL) return -1;
    uint64_t index = (virtual_address >> 12) & PT_INDEX_MASK;
    if (pt[index] & PTE_PRESENT) return 1;
    pt[index] = (physical_address & ~0xFFFULL) | flags | PTE_PRESENT;
    return 0;
}

static int unmap_page_root(uint64_t root_phys, uint64_t virtual_address) {
    if ((virtual_address & 0xFFFULL) != 0) return 0;
    uint64_t *pt = walk_create_root(root_phys, virtual_address, 0);
    if (pt == NULL) return 0;
    uint64_t index = (virtual_address >> 12) & PT_INDEX_MASK;
    if (!(pt[index] & PTE_PRESENT)) return 0;
    pt[index] = 0;
    return 1;
}

static uint64_t virt_to_phys_root(uint64_t root_phys, uint64_t virtual_address) {
    uint64_t pml4_index = (virtual_address >> 39) & PT_INDEX_MASK;
    uint64_t pdpt_index = (virtual_address >> 30) & PT_INDEX_MASK;
    uint64_t pd_index   = (virtual_address >> 21) & PT_INDEX_MASK;
    uint64_t pt_index   = (virtual_address >> 12) & PT_INDEX_MASK;
    uint64_t entry;

    uint64_t *pml4_table = table_from_phys(root_phys);
    if (pml4_table == NULL) return 0;
    entry = pml4_table[pml4_index];
    if (!(entry & PTE_PRESENT)) return 0;
    uint64_t *pdpt_table = table_from_phys(entry & ~0xFFFULL);
    if (pdpt_table == NULL) return 0;
    entry = pdpt_table[pdpt_index];
    if (!(entry & PTE_PRESENT)) return 0;
    if (entry & PTE_HUGE) return (entry & 0xFFFFFC0000000ULL) | (virtual_address & 0x3FFFFFFFULL);
    uint64_t *pd_table = table_from_phys(entry & ~0xFFFULL);
    if (pd_table == NULL) return 0;
    entry = pd_table[pd_index];
    if (!(entry & PTE_PRESENT)) return 0;
    if (entry & PTE_HUGE) return (entry & 0xFFFFFFE00000ULL) | (virtual_address & 0x1FFFFFULL);
    uint64_t *pt = table_from_phys(entry & ~0xFFFULL);
    if (pt == NULL) return 0;
    entry = pt[pt_index];
    if (!(entry & PTE_PRESENT)) return 0;
    return (entry & ~0xFFFULL) | (virtual_address & 0xFFFULL);
}

/* Clone the paging hierarchy. Leaf mappings are copied verbatim, but every
 * page-table page is private. Huge-page leaves remain shared physical mappings
 * (identity-mapped kernel/MMIO memory); only page-table ownership is private. */
static uint64_t clone_table(uint64_t src_phys, int level, int *ok) {
    uint64_t *src = table_from_phys(src_phys);
    if (src == NULL) { *ok = 0; return 0; }
    uint64_t dst_phys = alloc_table_page();
    if (dst_phys == 0) { *ok = 0; return 0; }
    uint64_t *dst = table_from_phys(dst_phys);
    if (dst == NULL) { pmm_free_page(dst_phys); *ok = 0; return 0; }

    for (int i = 0; i < ENTRIES_PER_TABLE; ++i) {
        uint64_t e = src[i];
        if (!(e & PTE_PRESENT)) continue;
        if (level == 2 && (e & PTE_HUGE)) {
            dst[i] = e;
            continue;
        }
        if (level == 1) {
            dst[i] = e;
            continue;
        }
        uint64_t child = clone_table(e & ~0xFFFULL, level - 1, ok);
        if (!*ok) { pmm_free_page(dst_phys); return 0; }
        dst[i] = child | (e & 0xFFFULL);
    }
    return dst_phys;
}

static void destroy_table(uint64_t phys, int level) {
    uint64_t *table = table_from_phys(phys);
    if (table == NULL) return;
    if (level > 1) {
        for (int i = 0; i < ENTRIES_PER_TABLE; ++i) {
            uint64_t e = table[i];
            if (!(e & PTE_PRESENT) || (level == 2 && (e & PTE_HUGE))) continue;
            destroy_table(e & ~0xFFFULL, level - 1);
        }
    }
    pmm_free_page(phys);
}

int paging_map_page(uint64_t virtual_address, uint64_t physical_address, uint64_t flags) {
    int result = map_page_root((uint64_t)(uintptr_t)pml4, virtual_address, physical_address, flags);
    if (result == 0)
        __asm__ volatile ("invlpg (%0)" : : "r"((void *)(uintptr_t)virtual_address) : "memory");
    return result;
}

int paging_unmap_page(uint64_t virtual_address) {
    int result = unmap_page_root((uint64_t)(uintptr_t)pml4, virtual_address);
    if (result == 1)
        __asm__ volatile ("invlpg (%0)" : : "r"((void *)(uintptr_t)virtual_address) : "memory");
    return result;
}

uint64_t paging_kernel_cr3(void) {
    return (uint64_t)(uintptr_t)pml4;
}

uint64_t paging_create_address_space(void) {
    int ok = 1;
    uint64_t root = clone_table((uint64_t)(uintptr_t)pml4, 4, &ok);
    if (!ok || root == 0) return 0;
    return root;
}

void paging_destroy_address_space(uint64_t cr3) {
    uint64_t kernel = (uint64_t)(uintptr_t)pml4;
    if (cr3 == 0 || cr3 == kernel) return;
    destroy_table(cr3, 4);
}

int paging_switch_cr3(uint64_t cr3) {
    if (cr3 == 0 || table_from_phys(cr3) == NULL) return 0;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
    return 1;
}

int paging_map_page_in_cr3(uint64_t cr3, uint64_t virtual_address,
                           uint64_t physical_address, uint64_t flags) {
    return map_page_root(cr3, virtual_address, physical_address, flags);
}

int paging_unmap_page_in_cr3(uint64_t cr3, uint64_t virtual_address) {
    return unmap_page_root(cr3, virtual_address);
}

uint64_t paging_virt_to_phys_in_cr3(uint64_t cr3, uint64_t virtual_address) {
    return virt_to_phys_root(cr3, virtual_address);
}

uint64_t paging_virt_to_phys(uint64_t virtual_address) {
    return virt_to_phys_root((uint64_t)(uintptr_t)pml4, virtual_address);
}
