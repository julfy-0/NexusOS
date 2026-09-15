#ifndef NEXUSOS_SYSCALL_H
#define NEXUSOS_SYSCALL_H
#include <stdint.h>

enum { NEXUS_SYS_NOP = 0, NEXUS_SYS_EXIT = 1, NEXUS_SYS_GETPID = 2 };

/* User ABI for the temporary INT 0x80 entry path:
 *   RAX = syscall number
 *   RDI/RSI/RDX = arguments 0..2
 *   RAX = return value
 *
 * The entry gate is deliberately explicit rather than using SYSCALL/SYSRET:
 * this keeps the first user transition dependent only on the already-tested
 * IDT/TSS machinery. */
void syscall_init(void);
int syscall_ready(void);
uint64_t syscall_dispatch(uint64_t number, uint64_t a0, uint64_t a1, uint64_t a2);

#define NEXUS_SYSCALL_ACTION_RETURN 0ULL
#define NEXUS_SYSCALL_ACTION_EXIT   1ULL

uint64_t syscall_interrupt_handler(uint64_t *frame);

#endif
