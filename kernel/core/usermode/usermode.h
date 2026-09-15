#ifndef NEXUSOS_USERMODE_H
#define NEXUSOS_USERMODE_H

#include <stdint.h>

/* Ring-3 transition ABI for x86_64. The selectors are owned by gdt.h. */
typedef struct {
    uint64_t entry;
    uint64_t user_stack;
    uint64_t kernel_stack;
} nexus_user_process_t;

void usermode_init(void);
int usermode_ready(void);

/* Validate and record the execution context of a process. This does not map
 * memory; the address-space block remains responsible for that contract. */
int usermode_prepare(uint64_t pid, uint64_t entry,
                     uint64_t user_stack, uint64_t kernel_stack);

/* Enter CPL3 through an architectural IRETQ frame. Returns only on invalid
 * input. A successful transition is expected to return through an interrupt
 * gate, using TSS.RSP0 as the kernel stack. */
int usermode_enter(uint64_t pid);

#endif
