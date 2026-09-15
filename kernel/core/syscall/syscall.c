#include "syscall.h"
#include "process.h"
#include "scheduler.h"
#include <stdint.h>

/* Saved GPR layout produced by syscall_entry.S. The five hardware words at
 * the end are the CPL3 IRETQ frame. */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, user_rsp, ss;
} __attribute__((packed)) syscall_interrupt_frame_t;

static int g_ready;

void syscall_init(void) {
    g_ready = 1;
}

int syscall_ready(void) { return g_ready; }

uint64_t syscall_dispatch(uint64_t number, uint64_t a0, uint64_t a1, uint64_t a2) {
    (void)a1;
    (void)a2;
    if (!g_ready) return (uint64_t)-1;

    switch (number) {
        case NEXUS_SYS_NOP:
            return 0;
        case NEXUS_SYS_GETPID: {
            nexus_process_t *p = process_current();
            return p ? p->pid : 0;
        }
        case NEXUS_SYS_EXIT:
            /* EXIT already has a kernel dispatcher, but termination currently
             * requires scheduler-owned user processes. Do not iretq into an
             * EXITED process from this first entry/return implementation. */
            (void)a0;
            return (uint64_t)-1;
        default:
            return (uint64_t)-1;
    }
}

static int syscall_frame_valid(const syscall_interrupt_frame_t *f) {
    if (!f) return 0;
    /* Only CPL3 may use this gate. Refuse malformed return selectors and
     * non-canonical user RIP/RSP before attempting IRETQ. */
    if ((f->cs & 3U) != 3U || (f->ss & 3U) != 3U) return 0;
    if ((f->cs & ~7ULL) != (0x23ULL & ~7ULL) ||
        (f->ss & ~7ULL) != (0x1BULL & ~7ULL)) return 0;
    if (f->rip >= PROCESS_USER_LIMIT || f->user_rsp >= PROCESS_USER_LIMIT) return 0;
    if (f->rip < PROCESS_USER_BASE || f->user_rsp < PROCESS_USER_BASE) return 0;
    return 1;
}

uint64_t syscall_interrupt_handler(uint64_t *frame) {
    syscall_interrupt_frame_t *f = (syscall_interrupt_frame_t *)frame;
    if (!g_ready || !syscall_frame_valid(f)) {
        f->rax = (uint64_t)-1;
        return NEXUS_SYSCALL_ACTION_RETURN;
    }

    /* EXIT is the one syscall allowed to terminate the current user process.
     * The handler marks the process exited and hands ownership to the
     * scheduler; the assembly epilogue must therefore NOT IRETQ into the
     * destroyed user context. All other syscalls remain non-blocking and do
     * not mutate scheduler state. */
    if (f->rax == NEXUS_SYS_EXIT) {
        nexus_process_t *p = process_current();
        if (!p || p->scheduler_thread_id == 0 ||
            !process_exit(p->pid)) {
            f->rax = (uint64_t)-1;
            return NEXUS_SYSCALL_ACTION_RETURN;
        }
        thread_exit();
        return NEXUS_SYSCALL_ACTION_EXIT;
    }

    f->rax = syscall_dispatch(f->rax, f->rdi, f->rsi, f->rdx);
    return NEXUS_SYSCALL_ACTION_RETURN;
}
