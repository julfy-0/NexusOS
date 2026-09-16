#include "stdin.h"
#include "process.h"
#include "scheduler.h"
#include "sync.h"

#define USER_STDIN_CAPACITY 512u

static uint8_t g_buffer[USER_STDIN_CAPACITY];
static uint32_t g_head;
static uint32_t g_tail;
static uint64_t g_owner_pid;
static uint64_t g_sequence;
static int g_ready;
static nexus_wait_queue_t g_waiters;
static nexus_spinlock_t g_lock;

static uint64_t irq_save_stdin(void) {
    return spinlock_lock_irqsave(&g_lock);
}

static void irq_restore_stdin(uint64_t flags) {
    spinlock_unlock_irqrestore(&g_lock, flags);
}

static uint32_t buffered_count_locked(void) {
    if (g_head >= g_tail) return g_head - g_tail;
    return USER_STDIN_CAPACITY - g_tail + g_head;
}

static int owner_valid_locked(void) {
    if (g_owner_pid == 0) return 0;
    nexus_process_t *p = process_get(g_owner_pid);
    if (!p || p->state == PROCESS_ZOMBIE || p->state == PROCESS_UNUSED) {
        g_owner_pid = 0;
        g_head = g_tail = 0;
        return 0;
    }
    return 1;
}

void userspace_stdin_init(void) {
    g_head = 0;
    g_tail = 0;
    g_owner_pid = 0;
    g_sequence = 0;
    scheduler_wait_queue_init(&g_waiters);
    spinlock_init(&g_lock);
    g_ready = 1;
}

int userspace_stdin_ready(void) { return g_ready != 0; }

int userspace_stdin_bind(uint64_t pid) {
    if (!g_ready || pid == 0) return 0;
    uint64_t flags = irq_save_stdin();
    if (!owner_valid_locked()) {
        g_owner_pid = pid;
        g_head = g_tail = 0;
        irq_restore_stdin(flags);
        return 1;
    }
    int ok = (g_owner_pid == pid);
    irq_restore_stdin(flags);
    return ok;
}

void userspace_stdin_unbind(uint64_t pid) {
    if (!g_ready || pid == 0) return;
    uint64_t flags = irq_save_stdin();
    if (g_owner_pid == pid) {
        g_owner_pid = 0;
        g_head = g_tail = 0;
        while (g_waiters.count != 0) {
            uint64_t tid = scheduler_wait_queue_pop_locked(&g_waiters);
            scheduler_wake_thread_locked(tid);
        }
    }
    irq_restore_stdin(flags);
}

uint64_t userspace_stdin_owner(void) {
    if (!g_ready) return 0;
    uint64_t flags = irq_save_stdin();
    (void)owner_valid_locked();
    uint64_t pid = g_owner_pid;
    irq_restore_stdin(flags);
    return pid;
}

uint64_t userspace_stdin_available(uint64_t pid) {
    if (!g_ready || pid == 0) return 0;
    uint64_t flags = irq_save_stdin();
    if (!owner_valid_locked() || g_owner_pid != pid) {
        irq_restore_stdin(flags);
        return 0;
    }
    uint64_t count = buffered_count_locked();
    irq_restore_stdin(flags);
    return count;
}

int userspace_stdin_read(uint64_t pid, void *buffer, uint64_t size, uint64_t *out_read) {
    if (out_read) *out_read = 0;
    if (!g_ready || pid == 0 || !buffer || size == 0) return 0;

    uint64_t flags = irq_save_stdin();
    if (!owner_valid_locked() || g_owner_pid != pid) {
        irq_restore_stdin(flags);
        return 0;
    }

    uint64_t available = buffered_count_locked();
    if (available == 0) {
        irq_restore_stdin(flags);
        return 0;
    }
    if (size > available) size = available;

    uint8_t *dst = (uint8_t *)buffer;
    for (uint64_t i = 0; i < size; ++i) {
        dst[i] = g_buffer[g_tail];
        g_tail = (g_tail + 1u) % USER_STDIN_CAPACITY;
    }
    if (out_read) *out_read = size;
    irq_restore_stdin(flags);
    return 1;
}

int userspace_stdin_push(uint8_t c) {
    if (!g_ready) return 0;
    uint64_t flags = irq_save_stdin();
    if (!owner_valid_locked()) {
        irq_restore_stdin(flags);
        return 0;
    }

    uint32_t next = (g_head + 1u) % USER_STDIN_CAPACITY;
    if (next == g_tail) {
        /* Preserve already queued input rather than overwriting unread data. */
        irq_restore_stdin(flags);
        return 0;
    }

    g_buffer[g_head] = c;
    g_head = next;
    ++g_sequence;

    /* Wake one blocked reader. The actual bytes remain in the shared terminal
     * input buffer until that thread resumes and performs its READ syscall. */
    uint64_t tid = scheduler_wait_queue_pop_locked(&g_waiters);
    if (tid != 0) scheduler_wake_thread_locked(tid);

    irq_restore_stdin(flags);
    return 1;
}

int userspace_stdin_wait(uint64_t pid) {
    if (!g_ready || pid == 0 || scheduler_current_thread_id() == 0) return 0;
    if (!userspace_stdin_bind(pid)) return 0;

    for (;;) {
        uint64_t flags = irq_save_stdin();
        if (!owner_valid_locked() || g_owner_pid != pid) {
            irq_restore_stdin(flags);
            return 0;
        }

        if (buffered_count_locked() != 0) {
            irq_restore_stdin(flags);
            return 1;
        }

        uint64_t tid = scheduler_current_thread_id();
        scheduler_wait_queue_push_locked(&g_waiters, tid);
        irq_restore_stdin(flags);

        if (!thread_block_on_wait_queue(&g_waiters)) return 0;
        if (userspace_stdin_available(pid) != 0) return 1;
    }
}

uint32_t userspace_stdin_waiter_count(void) {
    if (!g_ready) return 0;
    uint64_t flags = irq_save_stdin();
    uint32_t count = g_waiters.count;
    irq_restore_stdin(flags);
    return count;
}

uint64_t userspace_stdin_sequence(void) { return g_sequence; }
