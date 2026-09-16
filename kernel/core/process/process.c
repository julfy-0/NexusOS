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
static uint64_t g_zombie_count;
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
    g_zombie_count = 0;
    spinlock_init(&g_process_lock);
    g_ready = 1;
}

int process_is_ready(void) { return g_ready; }

uint64_t process_create(uint64_t parent_pid) {
    if (!g_ready) return PROCESS_INVALID_PID;
    uint64_t flags = spinlock_lock_irqsave(&g_process_lock);
    for (uint64_t i = 0; i < PROCESS_MAX_COUNT; ++i) {
        if (g_processes[i].state == PROCESS_UNUSED) {
            nexus_process_t *p = &g_processes[i];
            p->pid = g_next_pid++;
            if (p->pid == PROCESS_INVALID_PID) p->pid = g_next_pid++;
            p->parent_pid = parent_pid;
            p->start_tick = scheduler_ticks();
            p->cpu_ticks = 0;
            p->heap_base = 0x0000000010000000ULL;
            p->heap_end = p->heap_base;
            p->memory_limit_bytes = 0;
            p->priority = 16;
            p->env_count = 0;
            p->name[0] = 0;
            p->cwd[0] = '/'; p->cwd[1] = 0;
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
            for (uint64_t j = 0; j < PROCESS_MAX_FDS; ++j) {
                p->fds[j].type = PROCESS_FD_UNUSED;
                p->fds[j].flags = 0;
                p->fds[j].object = 0;
                p->fds[j].offset = 0; p->fds[j].path[0] = 0;
            }
            p->fds[0].type = PROCESS_FD_STDIN_CONSOLE;
            p->fds[1].type = PROCESS_FD_STDOUT_CONSOLE;
            p->fds[2].type = PROCESS_FD_STDERR_CONSOLE;
            p->fds[0].offset = p->fds[1].offset = p->fds[2].offset = 0;
            p->fds[0].path[0] = p->fds[1].path[0] = p->fds[2].path[0] = 0;
            p->exit_code = 0;
            p->exit_reason = 0;
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
    if (!next || (next->state != PROCESS_READY && next->state != PROCESS_RUNNING)) {
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

int process_exit_with_code(uint64_t pid, uint64_t exit_code, uint32_t exit_reason) {
    uint64_t flags = spinlock_lock_irqsave(&g_process_lock);
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_UNUSED || p->state == PROCESS_ZOMBIE) {
        spinlock_unlock_irqrestore(&g_process_lock, flags);
        return 0;
    }

    /* Do not destroy the current process address space here. A user syscall or
     * exception is still executing on this process's kernel stack. The
     * scheduler reaps the process only after another TCB is running. */
    p->exit_code = exit_code;
    p->exit_reason = exit_reason;
    p->state = PROCESS_ZOMBIE;
    if (g_current_pid == pid) {
        /* Keep the PID associated with the current TCB until the scheduler
         * switches away. This preserves process identity during hand-off. */
    }
    if (g_process_count) --g_process_count;
    ++g_zombie_count;
    spinlock_unlock_irqrestore(&g_process_lock, flags);
    return 1;
}

int process_exit(uint64_t pid) {
    return process_exit_with_code(pid, 0, 0);
}

int process_reap(uint64_t pid, uint64_t scheduler_thread_id) {
    uint64_t flags;
    nexus_process_t *p;
    uint64_t cr3;

    if (pid == PROCESS_INVALID_PID) return 0;
    flags = spinlock_lock_irqsave(&g_process_lock);
    p = process_get(pid);
    if (!p || p->state != PROCESS_ZOMBIE ||
        (scheduler_thread_id != 0 && p->scheduler_thread_id != scheduler_thread_id) ||
        (scheduler_thread_id == 0 && p->scheduler_thread_id != 0)) {
        spinlock_unlock_irqrestore(&g_process_lock, flags);
        return 0;
    }

    /* The scheduler calls this only after the zombie TCB stopped running, so
     * its private CR3 and user pages can finally be destroyed safely. */
    process_unmap_user_memory(pid);
    cr3 = p->address_space_cr3;
    p->address_space_cr3 = 0;
    if (cr3) paging_destroy_address_space(cr3);

    p->kernel_stack = 0;
    p->scheduler_thread_id = 0;
    for (uint64_t j = 0; j < PROCESS_MAX_FDS; ++j) {
        p->fds[j].type = PROCESS_FD_UNUSED;
        p->fds[j].flags = 0;
        p->fds[j].object = 0; p->fds[j].offset = 0; p->fds[j].path[0] = 0;
    }
    p->user_entry = 0;
    p->user_stack_base = 0;
    p->user_stack_size = 0;
    p->exit_code = 0;
    p->exit_reason = 0;
    p->parent_pid = 0;
    p->start_tick = 0; p->cpu_ticks = 0; p->heap_base = 0; p->heap_end = 0; p->memory_limit_bytes = 0; p->priority = 0; p->env_count = 0;
    p->name[0] = 0; p->cwd[0] = 0;
    p->pid = PROCESS_INVALID_PID;
    p->flags = 0;
    p->state = PROCESS_UNUSED;
    if (g_zombie_count) --g_zombie_count;
    spinlock_unlock_irqrestore(&g_process_lock, flags);
    return 1;
}

uint64_t process_count(void) { return g_process_count; }
uint64_t process_zombie_count(void) { return g_zombie_count; }

int process_reserve_user_range(uint64_t pid, uint64_t base, uint64_t size) {
    if (!aligned_range(base, size)) return 0;
    uint64_t flags = spinlock_lock_irqsave(&g_process_lock);
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_ZOMBIE || p->user_pages_reserved != 0) {
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
    if (!p || p->state == PROCESS_ZOMBIE || p->user_page_count >= PROCESS_USER_MAX_PAGES)
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
    if (!p || p->state == PROCESS_ZOMBIE || p->user_page_count != 0) return 0;
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

int process_user_read(uint64_t pid, uint64_t virtual_address, void *dst, uint64_t size) {
    nexus_process_t *p = process_get(pid);
    uint8_t *out = (uint8_t *)dst;
    if (!p || p->state == PROCESS_ZOMBIE || !dst || size == 0 ||
        virtual_address < PROCESS_USER_BASE ||
        size > PROCESS_USER_LIMIT - virtual_address ||
        !process_user_range_valid(pid, virtual_address, size, 0)) return 0;

    for (uint64_t off = 0; off < size; ) {
        uint64_t va = virtual_address + off;
        uint64_t phys = paging_virt_to_phys_in_cr3(p->address_space_cr3, va);
        if (!phys) return 0;
        uint64_t chunk = PAGE_SIZE - (va & (PAGE_SIZE - 1));
        if (chunk > size - off) chunk = size - off;
        const uint8_t *src = (const uint8_t *)(uintptr_t)(phys & ~(PAGE_SIZE - 1ULL)) +
                             (va & (PAGE_SIZE - 1));
        for (uint64_t i = 0; i < chunk; ++i) out[off + i] = src[i];
        off += chunk;
    }
    return 1;
}

int process_user_write(uint64_t pid, uint64_t virtual_address, const void *src, uint64_t size) {
    nexus_process_t *p = process_get(pid);
    const uint8_t *in = (const uint8_t *)src;
    if (!p || p->state == PROCESS_ZOMBIE || !src || size == 0 ||
        virtual_address < PROCESS_USER_BASE ||
        size > PROCESS_USER_LIMIT - virtual_address ||
        !process_user_range_valid(pid, virtual_address, size, VMM_PAGE_WRITABLE)) return 0;

    for (uint64_t off = 0; off < size; ) {
        uint64_t va = virtual_address + off;
        uint64_t phys = paging_virt_to_phys_in_cr3(p->address_space_cr3, va);
        if (!phys) return 0;
        uint64_t chunk = PAGE_SIZE - (va & (PAGE_SIZE - 1));
        if (chunk > size - off) chunk = size - off;
        uint8_t *out = (uint8_t *)(uintptr_t)(phys & ~(PAGE_SIZE - 1ULL)) +
                       (va & (PAGE_SIZE - 1));
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

int process_user_range_valid(uint64_t pid, uint64_t virtual_address,
                              uint64_t size, uint64_t required_flags) {
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_ZOMBIE || size == 0 ||
        virtual_address < PROCESS_USER_BASE ||
        size > PROCESS_USER_LIMIT - virtual_address)
        return 0;

    for (uint64_t off = 0; off < size; ) {
        uint64_t va = virtual_address + off;
        uint64_t page = va & ~(PAGE_SIZE - 1ULL);
        int mapped = 0;
        uint64_t page_flags = 0;

        for (uint64_t i = 0; i < p->user_page_count; ++i) {
            if (p->user_page_va[i] == page) {
                mapped = 1;
                page_flags = p->user_page_flags[i];
                break;
            }
        }
        if (!mapped) return 0;
        if ((required_flags & VMM_PAGE_WRITABLE) &&
            !(page_flags & VMM_PAGE_WRITABLE)) return 0;
        if ((required_flags & VMM_PAGE_NX) &&
            !(page_flags & VMM_PAGE_NX)) return 0;
        if (!(required_flags & VMM_PAGE_NX) &&
            (page_flags & VMM_PAGE_NX)) return 0;

        uint64_t chunk = PAGE_SIZE - (va & (PAGE_SIZE - 1ULL));
        if (chunk > size - off) chunk = size - off;
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



int process_fd_is_valid(uint64_t pid, uint64_t fd) {
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_ZOMBIE || fd >= PROCESS_MAX_FDS) return 0;
    return p->fds[fd].type != PROCESS_FD_UNUSED;
}

int process_fd_is_writable(uint64_t pid, uint64_t fd) {
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_ZOMBIE || fd >= PROCESS_MAX_FDS) return 0;
    return p->fds[fd].type == PROCESS_FD_STDOUT_CONSOLE ||
           p->fds[fd].type == PROCESS_FD_STDERR_CONSOLE;
}

#include "vfs.h"

static void local_copy(char *dst, const char *src, uint64_t cap);

int process_fd_close(uint64_t pid, uint64_t fd) {
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_ZOMBIE || fd >= PROCESS_MAX_FDS ||
        p->fds[fd].type == PROCESS_FD_UNUSED) return 0;
    p->fds[fd].type = PROCESS_FD_UNUSED;
    p->fds[fd].flags = 0;
    p->fds[fd].object = 0;
    p->fds[fd].offset = 0;
    p->fds[fd].path[0] = 0;
    return 1;
}

int process_fd_open(uint64_t pid, const char *path, uint32_t flags) {
    nexus_process_t *p=process_get(pid); if(!p||p->state==PROCESS_ZOMBIE||!path||!path[0]) return -1;
    int fd=-1; for(int i=0;i<PROCESS_MAX_FDS;i++) if(p->fds[i].type==PROCESS_FD_UNUSED){fd=i;break;}
    if(fd<0) return -1;
    uint64_t size=0; int exists=(vfs_file_size_path(path,&size)==0);
    if(!exists && !(flags&4u)) return -1;
    if(!exists && (flags&4u)){ static const uint8_t empty[1]={0}; if(vfs_write_path(path,empty,0,0,0)<0) return -1; }
    if((flags&8u)){ static const uint8_t empty[1]={0}; if(vfs_write_path(path,empty,0,0,0)<0) return -1; size=0; }
    p->fds[fd].type=PROCESS_FD_VFS_FILE; p->fds[fd].flags=flags; p->fds[fd].object=0; p->fds[fd].offset=(flags&16u)?size:0;
    local_copy(p->fds[fd].path,path,sizeof(p->fds[fd].path)); return fd;
}

int process_fd_read(uint64_t pid,uint64_t fd,void *buffer,uint64_t size){ nexus_process_t*p=process_get(pid); if(!p||fd>=PROCESS_MAX_FDS||p->fds[fd].type!=PROCESS_FD_VFS_FILE||!buffer||size==0)return -1; uint64_t n=0; if(vfs_read_path(p->fds[fd].path,(uint8_t*)buffer,size,p->fds[fd].offset,&n)<0)return -1; p->fds[fd].offset+=n; return (int)n; }
int process_fd_write(uint64_t pid,uint64_t fd,const void *buffer,uint64_t size){ nexus_process_t*p=process_get(pid); if(!p||fd>=PROCESS_MAX_FDS||p->fds[fd].type!=PROCESS_FD_VFS_FILE||!(p->fds[fd].flags&2u)||!buffer)return -1; uint64_t n=0; if(vfs_write_path(p->fds[fd].path,(const uint8_t*)buffer,size,p->fds[fd].offset,&n)<0)return -1; p->fds[fd].offset+=n; return (int)n; }
int process_fd_seek(uint64_t pid,uint64_t fd,int64_t off,uint32_t whence,uint64_t*out){ nexus_process_t*p=process_get(pid); if(!p||fd>=PROCESS_MAX_FDS||p->fds[fd].type!=PROCESS_FD_VFS_FILE)return 0; uint64_t size=0; if(vfs_file_size_path(p->fds[fd].path,&size)<0)return 0; int64_t base=(whence==0)?0:(whence==1?(int64_t)p->fds[fd].offset:(int64_t)size); int64_t pos=base+off; if(pos<0)return 0; p->fds[fd].offset=(uint64_t)pos; if(out)*out=(uint64_t)pos; return 1; }
int process_fd_tell(uint64_t pid,uint64_t fd,uint64_t*out){ nexus_process_t*p=process_get(pid); if(!p||fd>=PROCESS_MAX_FDS||p->fds[fd].type!=PROCESS_FD_VFS_FILE)return 0; if(out)*out=p->fds[fd].offset; return 1; }

/* 0.6.0 process metadata / FD extensions. */
static void local_copy(char *dst, const char *src, uint64_t cap) {
    if (!dst || cap == 0) return;
    uint64_t i = 0;
    if (src) while (src[i] && i + 1 < cap) { dst[i] = src[i]; ++i; }
    dst[i] = 0;
}
static int local_eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a || *b) { if (*a++ != *b++) return 0; }
    return 1;
}

int process_set_name(uint64_t pid, const char *name) {
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_ZOMBIE || !name || !name[0]) return 0;
    local_copy(p->name, name, PROCESS_NAME_LEN);
    return 1;
}
int process_get_name(uint64_t pid, char *out, uint64_t size) {
    nexus_process_t *p = process_get(pid);
    if (!p || !out || size == 0) return 0;
    local_copy(out, p->name, size); return 1;
}
int process_set_priority(uint64_t pid, uint32_t priority) {
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_ZOMBIE || priority > 31) return 0;
    p->priority = priority; return 1;
}
uint32_t process_get_priority(uint64_t pid) {
    nexus_process_t *p = process_get(pid); return p ? p->priority : 0;
}
int process_set_env(uint64_t pid, const char *key, const char *value) {
    nexus_process_t *p = process_get(pid);
    if (!p || p->state == PROCESS_ZOMBIE || !key || !key[0]) return 0;
    for (uint32_t i=0;i<p->env_count;i++) if (local_eq(p->env[i].key,key)) { local_copy(p->env[i].value,value?value:"",PROCESS_ENV_VALUE_LEN); return 1; }
    if (p->env_count >= PROCESS_ENV_COUNT) return 0;
    local_copy(p->env[p->env_count].key,key,PROCESS_ENV_KEY_LEN);
    local_copy(p->env[p->env_count].value,value?value:"",PROCESS_ENV_VALUE_LEN);
    p->env_count++; return 1;
}
int process_unset_env(uint64_t pid, const char *key) {
    nexus_process_t *p = process_get(pid); if (!p || !key) return 0;
    for (uint32_t i=0;i<p->env_count;i++) if (local_eq(p->env[i].key,key)) {
        for (uint32_t j=i+1;j<p->env_count;j++) p->env[j-1]=p->env[j];
        p->env_count--; return 1;
    }
    return 0;
}
int process_get_env(uint64_t pid, const char *key, char *out, uint64_t size) {
    nexus_process_t *p = process_get(pid); if (!p || !key || !out || size==0) return 0;
    for (uint32_t i=0;i<p->env_count;i++) if (local_eq(p->env[i].key,key)) { local_copy(out,p->env[i].value,size); return 1; }
    out[0]=0; return 0;
}
int process_get_cwd(uint64_t pid, char *out, uint64_t size) {
    nexus_process_t *p = process_get(pid); if (!p || !out || size==0) return 0; local_copy(out,p->cwd,size); return 1;
}
int process_set_cwd(uint64_t pid, const char *cwd) {
    nexus_process_t *p = process_get(pid); if (!p || p->state==PROCESS_ZOMBIE || !cwd || cwd[0]!='/') return 0; local_copy(p->cwd,cwd,PROCESS_CWD_LEN); return 1;
}
void process_account_cpu_tick(uint64_t pid) { nexus_process_t *p=process_get(pid); if (p && p->state==PROCESS_RUNNING) p->cpu_ticks++; }
uint64_t process_cpu_ticks(uint64_t pid) { nexus_process_t *p=process_get(pid); return p?p->cpu_ticks:0; }
uint64_t process_start_tick(uint64_t pid) { nexus_process_t *p=process_get(pid); return p?p->start_tick:0; }

int process_unmap_user_range(uint64_t pid, uint64_t base, uint64_t size) {
    nexus_process_t *p=process_get(pid); if(!p||size==0||(base&(PAGE_SIZE-1))||(size&(PAGE_SIZE-1)))return 0;
    uint64_t end=base+size; if(end<base)return 0;
    for(uint64_t i=0;i<p->user_page_count;) {
        uint64_t va=p->user_page_va[i];
        if(va>=base&&va<end){ uint64_t phys=p->user_page_phys[i]; if(p->address_space_cr3)paging_unmap_page_in_cr3(p->address_space_cr3,va); if(phys)pmm_free_page(phys);
            uint64_t last=p->user_page_count-1; p->user_page_va[i]=p->user_page_va[last]; p->user_page_phys[i]=p->user_page_phys[last]; p->user_page_flags[i]=p->user_page_flags[last]; p->user_page_count--; continue; }
        ++i;
    }
    return 1;
}

uint64_t process_alloc_user_range(uint64_t pid, uint64_t pages, uint64_t flags) {
    nexus_process_t *p=process_get(pid); if(!p||p->state==PROCESS_ZOMBIE||pages==0) return 0;
    if(p->user_page_count+pages>PROCESS_USER_MAX_PAGES) return 0;
    uint64_t base=p->heap_end; if(base<PROCESS_USER_BASE||base>=PROCESS_USER_LIMIT) base=0x0000000010000000ULL;
    uint64_t bytes=pages*PAGE_SIZE; if(bytes/pages!=PAGE_SIZE||base>PROCESS_USER_LIMIT-bytes)return 0;
    for(uint64_t i=0;i<pages;i++) if(!process_map_user_page(pid,base+i*PAGE_SIZE,flags)) { process_unmap_user_range(pid,base,i*PAGE_SIZE); return 0; }
    p->heap_end=base+bytes; return base;
}
