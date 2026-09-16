#ifndef NEXUSOS_SYSCALL_H
#define NEXUSOS_SYSCALL_H
#include <stdint.h>

enum {
    NEXUS_SYS_NOP = 0,
    NEXUS_SYS_EXIT = 1,
    NEXUS_SYS_GETPID = 2,
    NEXUS_SYS_WRITE = 3,
    NEXUS_SYS_CLOSE = 4,
    NEXUS_SYS_GETPPID = 5,
    NEXUS_SYS_OPEN = 6,
    NEXUS_SYS_READ = 7,
    NEXUS_SYS_SEEK = 8,
    NEXUS_SYS_TELL = 9,
    NEXUS_SYS_STAT = 10,
    NEXUS_SYS_MKDIR = 11,
    NEXUS_SYS_RMDIR = 12,
    NEXUS_SYS_UNLINK = 13,
    NEXUS_SYS_RENAME = 14,
    NEXUS_SYS_GETCWD = 15,
    NEXUS_SYS_CHDIR = 16,
    NEXUS_SYS_PROCESS_ENUM = 17,
    NEXUS_SYS_WAITPID = 18,
    NEXUS_SYS_GETPRIORITY = 19,
    NEXUS_SYS_SETPRIORITY = 20,
    NEXUS_SYS_GETNAME = 21,
    NEXUS_SYS_SETNAME = 22,
    NEXUS_SYS_EXEC = 23,
    NEXUS_SYS_MMAP = 24,
    NEXUS_SYS_MUNMAP = 25,
    NEXUS_SYS_BRK = 26
};

void syscall_init(void);
int syscall_ready(void);
uint64_t syscall_dispatch(uint64_t number, uint64_t a0, uint64_t a1, uint64_t a2);

#define NEXUS_SYSCALL_ACTION_RETURN 0ULL
#define NEXUS_SYSCALL_ACTION_EXIT   1ULL

uint64_t syscall_interrupt_handler(uint64_t *frame);

#endif
