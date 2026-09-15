#include "process.h"
#include "sync.h"
#include "vmm.h"
#include "pmm.h"
#include "paging.h"

#define PAGE_SIZE 4096ULL

static nexus_process_t g_processes[PROCESS_MAX_COUNT];
static uint64_t g_next_pid = 1;
static uint64_t g_current_pid;
static uint64_t g_process_count;
static int g_ready;
static nexus_spinlock_t g_process_lock;

static int aligned_range(uint64_t base, uint64_t size) {
    if (size == 0 || (base & (PAGE_SIZE - 1)) != 0 ||
        (size & (PAGE_SIZE - 1)) != 0) return 0;
    if (base < PROCESS_USER_BASE || base >= PROCESS_USER_LIMIT) return 0;
    if (size > PROCESS_USER_LIMIT - base) return 0;
    return 1;
}

void process_init(void) {
    for (uint64_t i = 0; i < PROCESS_MAX_COUNT; ++i)
        g_processes[i].state = PROCESS_UNUSED;
    g_next_pid = 1;
    g_current_pid = PROCESS_INVALID_PID;
    g_process_count = 0;
    spinlock_init(&g_process_lock);
    g_ready = 1;
}

int process_is_ready(void) { return g_ready; }

uint64_t process_create(uint64_t parent_pid) {
    if (!g_ready) return PROCESS_INVALID_PID;
    uint64_t flags = spinlock_lock_irqsave(&g_process_lock);
    for (uint64_t i = 0; i < PROCESS_MAX_COUNT; ++i) {
        if (g_processes[i].state == PROCESS_UNUSED || g_processes[i].state == PROCESS_EXITED) {
            nexus_process_t *p = &g_processes[i];
            p->pid = g_next_pid++;
            if (p->pid == PROCESS_INVALID_PID) p->pid = g_next_pid++;
            p->parent_pid = parent_pid;
            p->address_space_cr3 = paging_create_address_space();
            if (p->address_space_cr3 == 0) {
                p->state = PROCESS_UNUSED;
                spinlock_unlock_irqrestore(&g_process_lock, flags);
                return PROCESS_INVALID_PID;
            }
            p->user_entry = 0;
            p->user_stack_base = 0;
            p->user_stack_size = 0;
            p->user_pages_reserved = 0;
            p->user_page_base = 0; p->user_page_count = 0;
            for (uint64_t j = 0; j < PROCESS_USER_MAX_PAGES; ++j) {
                p->user_page_phys[j] = 0;
                p->user_page_va[j] = 0;
                p->user_page_flags[j] = 0;
            }
            p->kernel_stack = 0;
            p->scheduler_thread_id = 0;
            p->flags = 0;
            p->state = PROCESS_READY;
            ++g_process_count;
            uint64_t pid = p->pid;
            spinlock_unlock_irqrestore(&g_process_lock, flags);
            return pid;
        }
    }
    spinlock_unlock_irqrestore(&g_process_lock, flags);
    return PROCESS_INVALID_PID;
}

nexus_process_t *process_get(uint64_t pid) {
    if (!g_ready || pid == PROCESS_INVALID_PID) return 0;
    for (uint64_t i = 0; i < PROCESS_MAX_COUNT; ++i)
        if (g_processes[i].state != PROCESS_UNUSED && g_processes[i].pid == pid)
            return &g_processes[i];
    return 0;
}

int process_set_current(uint64_t pid) {
    uint64_t flags = spinlock_lock_irqsave(&g_process_lock);
    nexus_process_t *next = process_get(pid);
    if (!next || next->state == PROCESS_EXITED) {
        spinlock_unlock_irqrestore(&g_process_lock, flags);
        return 0;
    }
    nexus_process_t *old = process_get(g_current_pid);
    if (old && old != next && old->state == PROCESS_RUNNING) old->state = PROCESS_READY;
    next->state = PROCESS_RUNNING;
    g_current_pid = pid;
    spinlock_unlock_irqrestore(&g_process_lock, flags);
    return 1;
}

nexus_process_t *process_current(void) { return process_get(g_current_pid); }

void process_clear_current(void) {
    uint64_t flags = spinlock_lock_irqsave(&g_process_lock);
    nexus_process_t *old = process_get(g_current_pid);
    if (old && old->state == PROCESS_RUNNING) old->state = PROCESS_READY;
    g_current_pid = PROCESS_INVALID_PID;
    spinlock_unlock_irqrestore(&g_process_lock, flags);
}

int process_exit(uint64_t pid) {
    uint64_t flags = spinlock_lock_irqsave(&g_process_lock);
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_EXITED) {
        spinlock_unlock_irqrestore(&g_process_lock, flags);
        return 0;
    }
    process_unmap_user_memory(pid);
    if (p->address_space_cr3) {
        paging_destroy_address_space(p->address_space_cr3);
        p->address_space_cr3 = 0;
    }
    p->state = PROCESS_EXITED;
    if (g_current_pid == pid) g_current_pid = PROCESS_INVALID_PID;
    if (g_process_count) --g_process_count;
    spinlock_unlock_irqrestore(&g_process_lock, flags);
    return 1;
}

uint64_t process_count(void) { return g_process_count; }

int process_reserve_user_range(uint64_t pid, uint64_t base, uint64_t size) {
    if (!aligned_range(base, size)) return 0;
    uint64_t flags = spinlock_lock_irqsave(&g_process_lock);
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_EXITED || p->user_pages_reserved != 0) {
        spinlock_unlock_irqrestore(&g_process_lock, flags);
        return 0;
    }
    p->user_stack_base = base;
    p->user_stack_size = size;
    p->user_pages_reserved = size / PAGE_SIZE;
    spinlock_unlock_irqrestore(&g_process_lock, flags);
    return 1;
}

int process_map_user_page(uint64_t pid, uint64_t virtual_address, uint64_t flags) {
    if (!g_ready || (virtual_address & (PAGE_SIZE - 1)) != 0 ||
        virtual_address < PROCESS_USER_BASE || virtual_address >= PROCESS_USER_LIMIT)
        return 0;

    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_EXITED || p->user_page_count >= PROCESS_USER_MAX_PAGES)
        return 0;

    for (uint64_t i = 0; i < p->user_page_count; ++i)
        if (p->user_page_va[i] == virtual_address) return 0;

    uint64_t phys = pmm_alloc_page();
    if (!phys) return 0;
    if (!paging_map_page_in_cr3(p->address_space_cr3, virtual_address, phys,
                                 VMM_PAGE_PRESENT | VMM_PAGE_USER | flags)) {
        pmm_free_page(phys);
        return 0;
    }
    /* The kernel remains identity-mapped, so initialization can touch the
     * physical page without temporarily entering the process address space. */
    for (uint64_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); ++i)
        ((uint64_t *)(uintptr_t)phys)[i] = 0;

    uint64_t slot = p->user_page_count++;
    p->user_page_va[slot] = virtual_address;
    p->user_page_phys[slot] = phys;
    p->user_page_flags[slot] = flags;
    p->flags |= PROCESS_FLAG_USER_MEMORY;
    return 1;
}

int process_map_user_memory(uint64_t pid, uint64_t base, uint64_t pages) {
    if (pages == 0 || pages > PROCESS_USER_MAX_PAGES ||
        pages > (PROCESS_USER_LIMIT - base) / PAGE_SIZE ||
        (base & (PAGE_SIZE - 1)) != 0)
        return 0;
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_EXITED || p->user_page_count != 0) return 0;
    for (uint64_t i = 0; i < pages; ++i) {
        if (!process_map_user_page(pid, base + i * PAGE_SIZE, VMM_PAGE_WRITABLE)) {
            process_unmap_user_memory(pid);
            return 0;
        }
    }
    p->user_page_base = base;
    p->user_pages_reserved = pages;
    return 1;
}

int process_user_copy(uint64_t pid, uint64_t virtual_address, const void *src, uint64_t size) {
    nexus_process_t *p = process_get(pid);
    const uint8_t *in = (const uint8_t *)src;
    if (!p || !src || size == 0 || virtual_address < PROCESS_USER_BASE ||
        size > PROCESS_USER_LIMIT - virtual_address) return 0;
    for (uint64_t off = 0; off < size; ) {
        uint64_t va = virtual_address + off;
        uint64_t phys = paging_virt_to_phys_in_cr3(p->address_space_cr3, va);
        if (!phys) return 0;
        uint64_t chunk = PAGE_SIZE - (va & (PAGE_SIZE - 1));
        if (chunk > size - off) chunk = size - off;
        uint8_t *out = (uint8_t *)(uintptr_t)(phys & ~(PAGE_SIZE - 1)) + (va & (PAGE_SIZE - 1));
        for (uint64_t i = 0; i < chunk; ++i) out[i] = in[off + i];
        off += chunk;
    }
    return 1;
}

int process_user_zero(uint64_t pid, uint64_t virtual_address, uint64_t size) {
    nexus_process_t *p = process_get(pid);
    if (!p || size == 0 || virtual_address < PROCESS_USER_BASE ||
        size > PROCESS_USER_LIMIT - virtual_address) return 0;
    for (uint64_t off = 0; off < size; ) {
        uint64_t va = virtual_address + off;
        uint64_t phys = paging_virt_to_phys_in_cr3(p->address_space_cr3, va);
        if (!phys) return 0;
        uint64_t chunk = PAGE_SIZE - (va & (PAGE_SIZE - 1));
        if (chunk > size - off) chunk = size - off;
        uint8_t *out = (uint8_t *)(uintptr_t)(phys & ~(PAGE_SIZE - 1)) + (va & (PAGE_SIZE - 1));
        for (uint64_t i = 0; i < chunk; ++i) out[i] = 0;
        off += chunk;
    }
    return 1;
}

int process_unmap_user_memory(uint64_t pid) {
    nexus_process_t *p = process_get(pid); if (!p) return 0;
    for (uint64_t i = 0; i < p->user_page_count; ++i) {
        uint64_t va = p->user_page_va[i];
        uint64_t phys = p->user_page_phys[i];
        if (p->address_space_cr3 && va) paging_unmap_page_in_cr3(p->address_space_cr3, va);
        if (phys) pmm_free_page(phys);
        p->user_page_va[i] = 0;
        p->user_page_phys[i] = 0;
        p->user_page_flags[i] = 0;
    }
    p->user_page_base = 0;
    p->user_page_count = 0;
    p->user_pages_reserved = 0;
    p->flags &= ~PROCESS_FLAG_USER_MEMORY;
    return 1;
}

