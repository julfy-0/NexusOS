#ifndef NEXUSOS_PROCESS_H
#define NEXUSOS_PROCESS_H

#include <stdint.h>

#define PROCESS_MAX_COUNT 8
#define PROCESS_INVALID_PID 0ULL
#define PROCESS_USER_BASE 0x0000000000400000ULL
#define PROCESS_USER_LIMIT 0x00007FFFFFFFF000ULL
#define PROCESS_FLAG_USER_CONTEXT 0x00000001U
#define PROCESS_FLAG_USER_MEMORY  0x00000002U
#define PROCESS_FLAG_SCHEDULER_OWNED 0x00000004U
#define PROCESS_USER_MAX_PAGES 16
#define PROCESS_USER_STACK_TOP 0x0000000080000000ULL
#define PROCESS_MAX_FDS 8

typedef enum {
    PROCESS_FD_UNUSED = 0,
    PROCESS_FD_STDIN_CONSOLE,
    PROCESS_FD_STDOUT_CONSOLE,
    PROCESS_FD_STDERR_CONSOLE
} process_fd_type_t;

typedef struct {
    process_fd_type_t type;
    uint32_t flags;
    uint64_t object;
} process_fd_t;

typedef enum {
    PROCESS_UNUSED = 0,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_ZOMBIE
} process_state_t;

typedef struct nexus_process {
    uint64_t pid, parent_pid, address_space_cr3;
    uint64_t user_entry, user_stack_base, user_stack_size, user_pages_reserved;
    uint64_t user_page_base, user_page_count, user_page_phys[PROCESS_USER_MAX_PAGES];
    uint64_t user_page_va[PROCESS_USER_MAX_PAGES];
    uint64_t user_page_flags[PROCESS_USER_MAX_PAGES];
    uint64_t kernel_stack;
    uint64_t scheduler_thread_id;
    process_fd_t fds[PROCESS_MAX_FDS];
    uint64_t exit_code;
    uint32_t exit_reason;
    process_state_t state;
    uint32_t flags;
} nexus_process_t;

void process_init(void); int process_is_ready(void); uint64_t process_create(uint64_t parent_pid);
nexus_process_t *process_get(uint64_t pid); int process_set_current(uint64_t pid);
void process_clear_current(void); nexus_process_t *process_current(void);
int process_exit(uint64_t pid);
int process_exit_with_code(uint64_t pid, uint64_t exit_code, uint32_t exit_reason);
int process_reap(uint64_t pid, uint64_t scheduler_thread_id);
uint64_t process_count(void);
uint64_t process_zombie_count(void); int process_reserve_user_range(uint64_t pid,uint64_t base,uint64_t size);
int process_map_user_page(uint64_t pid, uint64_t virtual_address, uint64_t flags);
int process_user_copy(uint64_t pid, uint64_t virtual_address, const void *src, uint64_t size);
int process_user_read(uint64_t pid, uint64_t virtual_address, void *dst, uint64_t size);
int process_user_write(uint64_t pid, uint64_t virtual_address, const void *src, uint64_t size);
int process_user_zero(uint64_t pid, uint64_t virtual_address, uint64_t size);
int process_fd_is_valid(uint64_t pid, uint64_t fd);
int process_fd_is_writable(uint64_t pid, uint64_t fd);
int process_fd_close(uint64_t pid, uint64_t fd);
int process_user_range_valid(uint64_t pid, uint64_t virtual_address, uint64_t size, uint64_t required_flags);
int process_map_user_memory(uint64_t pid,uint64_t base,uint64_t pages); int process_unmap_user_memory(uint64_t pid);
#endif
