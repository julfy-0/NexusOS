#include "usermode.h"
#include "gdt.h"
#include "process.h"

static int ready;
extern void arch_enter_user_mode(uint64_t entry, uint64_t user_stack);

void usermode_init(void) {
    ready = gdt_usermode_ready();
}

int usermode_ready(void) { return ready; }

int usermode_prepare(uint64_t pid, uint64_t entry,
                     uint64_t user_stack, uint64_t kernel_stack) {
    nexus_process_t *p;
    if (!ready || entry == 0 || user_stack == 0 || kernel_stack == 0)
        return 0;
    p = process_get(pid);
    if (!p || p->state == PROCESS_ZOMBIE)
        return 0;
    p->user_entry = entry;
    p->user_stack_base = user_stack;
    p->user_stack_size = 0;
    p->kernel_stack = kernel_stack;
    p->flags |= PROCESS_FLAG_USER_CONTEXT;
    return 1;
}

int usermode_enter(uint64_t pid) {
    nexus_process_t *p;
    if (!ready) return 0;
    p = process_get(pid);
    if (!p || p->state == PROCESS_ZOMBIE ||
        (p->flags & PROCESS_FLAG_USER_CONTEXT) == 0 ||
        p->user_entry == 0 || p->user_stack_base == 0 || p->kernel_stack == 0)
        return 0;

    /* TSS.RSP0 is the only kernel stack used when CPL3 enters an interrupt or
     * exception gate. No scheduler context switch is performed here. */
    gdt_set_kernel_stack(p->kernel_stack);
    if (!process_set_current(pid)) return 0;
    arch_enter_user_mode(p->user_entry, p->user_stack_base);
    return 0;
}
