#ifndef NEXUSOS_PROCESS_H
#define NEXUSOS_PROCESS_H

#include <stdint.h>
#include "capability.h"

#define PROCESS_MAX_COUNT 8
#define PROCESS_INVALID_PID 0ULL
#define PROCESS_USER_BASE 0x0000000000400000ULL
#define PROCESS_USER_LIMIT 0x00007FFFFFFFF000ULL
#define PROCESS_FLAG_USER_CONTEXT 0x00000001U
#define PROCESS_FLAG_USER_MEMORY  0x00000002U
#define PROCESS_FLAG_SCHEDULER_OWNED 0x00000004U
#define PROCESS_USER_MAX_PAGES 16
#define PROCESS_USER_STACK_TOP 0x0000000080000000ULL
#define PROCESS_MAX_FDS 16
#define PROCESS_NAME_LEN 32
#define PROCESS_CWD_LEN 128
#define PROCESS_ENV_COUNT 8
#define PROCESS_ENV_KEY_LEN 24
#define PROCESS_ENV_VALUE_LEN 64
#define PROCESS_MAX_CHANNELS 8

typedef enum {
    PROCESS_FD_UNUSED = 0,
    PROCESS_FD_STDIN_CONSOLE,
    PROCESS_FD_STDOUT_CONSOLE,
    PROCESS_FD_STDERR_CONSOLE,
    PROCESS_FD_VFS_FILE,
    PROCESS_FD_VFS_DIR
} process_fd_type_t;

typedef struct {
    process_fd_type_t type;
    uint32_t flags;
    uint64_t object;
    uint64_t offset;
    char path[128];
} process_fd_t;

typedef enum {
    PROCESS_UNUSED = 0,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_ZOMBIE
} process_state_t;

typedef struct nexus_process {
    uint64_t pid, parent_pid, address_space_cr3;
    nexus_capability_mask_t capabilities;
    uint64_t start_tick;
    uint64_t cpu_ticks;
    uint64_t heap_base;
    uint64_t heap_end;
    uint64_t memory_limit_bytes;
    uint32_t priority;
    uint32_t env_count;
    char name[PROCESS_NAME_LEN];
    char cwd[PROCESS_CWD_LEN];
    struct { char key[PROCESS_ENV_KEY_LEN]; char value[PROCESS_ENV_VALUE_LEN]; } env[PROCESS_ENV_COUNT];
    uint64_t user_entry, user_stack_base, user_stack_size, user_pages_reserved;
    uint64_t user_page_base, user_page_count, user_page_phys[PROCESS_USER_MAX_PAGES];
    uint64_t user_page_va[PROCESS_USER_MAX_PAGES];
    uint64_t user_page_flags[PROCESS_USER_MAX_PAGES];
    uint64_t kernel_stack;
    uint64_t scheduler_thread_id;
    process_fd_t fds[PROCESS_MAX_FDS];
    uint32_t ipc_handles[PROCESS_MAX_CHANNELS];
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
int process_fd_open(uint64_t pid, const char *path, uint32_t flags);
int process_fd_read(uint64_t pid, uint64_t fd, void *buffer, uint64_t size);
int process_fd_write(uint64_t pid, uint64_t fd, const void *buffer, uint64_t size);
int process_fd_seek(uint64_t pid, uint64_t fd, int64_t offset, uint32_t whence, uint64_t *out_offset);
int process_fd_tell(uint64_t pid, uint64_t fd, uint64_t *out_offset);
int process_set_name(uint64_t pid, const char *name);
int process_get_name(uint64_t pid, char *out, uint64_t size);
int process_set_priority(uint64_t pid, uint32_t priority);
int process_has_capability(uint64_t pid, uint64_t capability);
nexus_capability_mask_t process_get_capabilities(uint64_t pid);
int process_drop_capability(uint64_t pid, uint64_t capability);
int process_grant_capability(uint64_t pid, uint64_t capability);
uint32_t process_get_priority(uint64_t pid);
int process_set_env(uint64_t pid, const char *key, const char *value);
int process_unset_env(uint64_t pid, const char *key);
int process_get_env(uint64_t pid, const char *key, char *out, uint64_t size);
int process_get_cwd(uint64_t pid, char *out, uint64_t size);
int process_set_cwd(uint64_t pid, const char *cwd);
void process_account_cpu_tick(uint64_t pid);
void process_ipc_reset_handles(uint64_t pid);
uint64_t process_cpu_ticks(uint64_t pid);
uint64_t process_start_tick(uint64_t pid);
int process_user_range_valid(uint64_t pid, uint64_t virtual_address, uint64_t size, uint64_t required_flags);
int process_map_user_memory(uint64_t pid,uint64_t base,uint64_t pages); int process_unmap_user_memory(uint64_t pid);
int process_unmap_user_range(uint64_t pid, uint64_t base, uint64_t size);
uint64_t process_alloc_user_range(uint64_t pid, uint64_t pages, uint64_t flags);
#endif
