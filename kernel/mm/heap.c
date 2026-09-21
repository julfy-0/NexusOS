#include <stddef.h>
#include <stdint.h>
#include "heap.h"
#include "pmm.h"
#include "vmm.h"

#define PAGE_SIZE       4096ULL
#define HEAP_BASE       0xFFFF800000000000ULL
#define HEAP_LIMIT      0xFFFF900000000000ULL
#define HEAP_ALIGNMENT  16ULL
#define BLOCK_MAGIC     0x4E58484C424C4B31ULL /* "NXHLBLK1" */
#define BLOCK_FREE      0x01U

/* A deliberately simple kernel heap for the pre-process era of NexusOS.
 * Metadata is kept inside the mapped heap, so the heap has no dependency on
 * another allocator. Blocks are physically backed only when the virtual
 * arena grows.
 */
typedef struct heap_block {
    uint64_t magic;
    uint64_t size;              /* payload bytes */
    uint64_t capacity;          /* mapped payload bytes owned by this block */
    uint64_t flags;
    struct heap_block *prev;
    struct heap_block *next;
} heap_block_t;

#define HEADER_SIZE ((sizeof(heap_block_t) + (HEAP_ALIGNMENT - 1ULL)) & ~(HEAP_ALIGNMENT - 1ULL))

static heap_block_t *g_head;
static uint64_t g_heap_next;
static uint64_t g_heap_pages;
static uint64_t g_used_bytes;
static uint64_t g_free_bytes;
static uint64_t g_allocations;
static uint64_t g_corruption_count;
static int g_ready;

static uint64_t align_up_u64(uint64_t value, uint64_t alignment) {
    uint64_t mask = alignment - 1ULL;
    if (value > UINT64_MAX - mask) return UINT64_MAX;
    return (value + mask) & ~mask;
}

static size_t align_size(size_t size) {
    uint64_t aligned = align_up_u64((uint64_t)size, HEAP_ALIGNMENT);
    if (aligned == UINT64_MAX || aligned > (uint64_t)SIZE_MAX) return 0;
    return (size_t)aligned;
}

static uint64_t pages_for_block(size_t payload) {
    uint64_t total = HEADER_SIZE + (uint64_t)payload;
    return (total + PAGE_SIZE - 1ULL) / PAGE_SIZE;
}

static int interrupts_were_enabled(void) {
    uint64_t rflags;
    __asm__ volatile ("pushfq; pop %0" : "=r"(rflags));
    return (rflags & (1ULL << 9)) != 0;
}

static void heap_lock(void) {
    __asm__ volatile ("cli" ::: "memory");
}

static void heap_unlock(int restore_interrupts) {
    if (restore_interrupts) __asm__ volatile ("sti" ::: "memory");
}

static int map_new_pages(uint64_t start, uint64_t pages) {
    for (uint64_t i = 0; i < pages; i++) {
        uint64_t phys = pmm_alloc_page();
        if (phys == 0) {
            /* Roll back pages mapped during this growth operation. */
            for (uint64_t j = 0; j < i; j++) {
                uint64_t va = start + j * PAGE_SIZE;
                uint64_t old_phys = vmm_virt_to_phys(va);
                if (old_phys != 0) {
                    vmm_unmap_page(va);
                    pmm_free_page(old_phys & ~(PAGE_SIZE - 1ULL));
                }
            }
            return 0;
        }

        if (!vmm_map_page(start + i * PAGE_SIZE, phys,
                          VMM_PAGE_PRESENT | VMM_PAGE_WRITABLE)) {
            pmm_free_page(phys);
            for (uint64_t j = 0; j < i; j++) {
                uint64_t va = start + j * PAGE_SIZE;
                uint64_t old_phys = vmm_virt_to_phys(va);
                if (old_phys != 0) {
                    vmm_unmap_page(va);
                    pmm_free_page(old_phys & ~(PAGE_SIZE - 1ULL));
                }
            }
            return 0;
        }
    }
    return 1;
}

static void split_block(heap_block_t *block, size_t requested) {
    uint64_t remaining = block->capacity - (uint64_t)requested;
    if (remaining < HEADER_SIZE + HEAP_ALIGNMENT) {
        block->size = block->capacity;
        return;
    }

    uint8_t *new_addr = (uint8_t *)block + HEADER_SIZE + requested;
    heap_block_t *tail = (heap_block_t *)new_addr;
    tail->magic = BLOCK_MAGIC;
    tail->size = remaining - HEADER_SIZE;
    tail->capacity = tail->size;
    tail->flags = BLOCK_FREE;
    tail->prev = block;
    tail->next = block->next;

    if (tail->next != NULL) tail->next->prev = tail;
    block->next = tail;
    block->size = requested;
    block->capacity = requested;

    g_free_bytes += tail->size;
}

static void merge_with_next(heap_block_t *block) {
    heap_block_t *next = block->next;
    if (next == NULL || !(next->flags & BLOCK_FREE)) return;

    block->capacity += HEADER_SIZE + next->capacity;
    block->size = block->capacity;
    g_free_bytes += HEADER_SIZE;
    block->next = next->next;
    if (block->next != NULL) block->next->prev = block;
}

static int ptr_in_heap(uint64_t addr) {
    return addr >= HEAP_BASE && addr < g_heap_next;
}

static int block_basic_valid(const heap_block_t *block) {
    if (block == NULL) return 0;
    uint64_t addr = (uint64_t)(uintptr_t)block;
    if (!ptr_in_heap(addr)) return 0;
    if (block->magic != BLOCK_MAGIC) return 0;
    if (block->capacity == 0 || block->size > block->capacity) return 0;
    uint64_t end = addr + HEADER_SIZE + block->capacity;
    if (end < addr || end > g_heap_next) return 0;
    if (block->prev != NULL && !ptr_in_heap((uint64_t)(uintptr_t)block->prev)) return 0;
    if (block->next != NULL && !ptr_in_heap((uint64_t)(uintptr_t)block->next)) return 0;
    return 1;
}

int heap_validate(void) {
    if (!g_ready) return 0;
    uint64_t seen = 0;
    heap_block_t *prev = NULL;
    for (heap_block_t *block = g_head; block != NULL; block = block->next) {
        if (seen++ > 1000000ULL) {
            g_corruption_count++;
            return 0;
        }
        if (!block_basic_valid(block) || block->prev != prev) {
            g_corruption_count++;
            return 0;
        }
        if (block->next != NULL && block->next->prev != block) {
            g_corruption_count++;
            return 0;
        }
        if (block->flags & ~BLOCK_FREE) {
            g_corruption_count++;
            return 0;
        }
        prev = block;
    }
    if (g_head != NULL && g_head->prev != NULL) {
        g_corruption_count++;
        return 0;
    }
    return 1;
}

void heap_init(void) {
    g_head = NULL;
    g_heap_next = HEAP_BASE;
    g_heap_pages = 0;
    g_used_bytes = 0;
    g_free_bytes = 0;
    g_allocations = 0;
    g_corruption_count = 0;
    g_ready = vmm_page_size() == PAGE_SIZE && pmm_total_pages() != 0;
}

void *kmalloc(size_t size) {
    if (!g_ready || size == 0) return NULL;

    size_t requested = align_size(size);
    if (requested == 0) return NULL;

    int restore_if = interrupts_were_enabled();
    heap_lock();
    if (!heap_validate()) {
        heap_unlock(restore_if);
        return NULL;
    }

    /* First-fit. Free blocks keep their virtual pages mapped for fast reuse. */
    for (heap_block_t *block = g_head; block != NULL; block = block->next) {
        if ((block->flags & BLOCK_FREE) && block->capacity >= requested) {
            g_free_bytes -= block->capacity;
            block->flags &= ~BLOCK_FREE;
            block->size = requested;
            split_block(block, requested);
            g_used_bytes += requested;
            g_allocations++;
            heap_unlock(restore_if);
            return (uint8_t *)block + HEADER_SIZE;
        }
    }

    uint64_t pages = pages_for_block(requested);
    uint64_t capacity = pages * PAGE_SIZE - HEADER_SIZE;
    if (pages == 0 || capacity < requested || g_heap_next > HEAP_LIMIT ||
        pages > (HEAP_LIMIT - g_heap_next) / PAGE_SIZE) {
        heap_unlock(restore_if);
        return NULL;
    }

    uint64_t block_va = g_heap_next;
    if (!map_new_pages(block_va, pages)) {
        heap_unlock(restore_if);
        return NULL;
    }

    heap_block_t *block = (heap_block_t *)(uintptr_t)block_va;
    block->magic = BLOCK_MAGIC;
    block->size = requested;
    block->capacity = capacity;
    block->flags = 0;
    block->prev = NULL;
    block->next = NULL;

    if (g_head == NULL) {
        g_head = block;
    } else {
        heap_block_t *tail = g_head;
        while (tail->next != NULL) tail = tail->next;
        tail->next = block;
        block->prev = tail;
    }

    g_heap_next += pages * PAGE_SIZE;
    g_heap_pages += pages;
    g_used_bytes += requested;
    g_allocations++;

    heap_unlock(restore_if);
    return (uint8_t *)block + HEADER_SIZE;
}

void kfree(void *ptr) {
    if (!g_ready || ptr == NULL) return;

    uint8_t *raw = (uint8_t *)ptr;
    if ((uint64_t)(uintptr_t)raw < HEAP_BASE + HEADER_SIZE ||
        (uint64_t)(uintptr_t)raw >= g_heap_next) return;

    heap_block_t *block = (heap_block_t *)(raw - HEADER_SIZE);
    if (block->magic != BLOCK_MAGIC || (block->flags & BLOCK_FREE)) return;

    int restore_if = interrupts_were_enabled();
    heap_lock();
    if (!heap_validate()) {
        heap_unlock(restore_if);
        return;
    }

    if (block->magic != BLOCK_MAGIC || (block->flags & BLOCK_FREE)) {
        heap_unlock(restore_if);
        return;
    }

    block->flags |= BLOCK_FREE;
    if (g_used_bytes >= block->size) g_used_bytes -= block->size;
    g_free_bytes += block->capacity;
    if (g_allocations != 0) g_allocations--;

    /* Merge forward first, then backward. This keeps the free list compact. */
    merge_with_next(block);
    if (block->prev != NULL && (block->prev->flags & BLOCK_FREE)) {
        heap_block_t *prev = block->prev;
        merge_with_next(prev);
        block = prev;
    }

    heap_unlock(restore_if);
}

int heap_is_ready(void) { return g_ready; }
uint64_t heap_virtual_base(void) { return HEAP_BASE; }
uint64_t heap_virtual_end(void) { return g_heap_next; }
uint64_t heap_mapped_pages(void) { return g_heap_pages; }
uint64_t heap_used_bytes(void) { return g_used_bytes; }
uint64_t heap_free_bytes(void) { return g_free_bytes; }
uint64_t heap_allocations(void) { return g_allocations; }
uint64_t heap_corruption_count(void) { return g_corruption_count; }
