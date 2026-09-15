#include "scheduler.h"
#include <stddef.h>
#include <stdint.h>
#include "heap.h"
#include "process.h"
#include "usermode.h"
#include "paging.h"

#define SCHED_MAX_THREADS NEXUS_SCHEDULER_MAX_THREADS
#define SCHED_STACK_SIZE 16384u
#define THREAD_MAGIC 0x4E58544852454144ULL /* "NXTHREAD" */
#define THREAD_READY 0x01u
#define THREAD_RUNNING 0x02u
#define THREAD_SLEEPING 0x04u
#define THREAD_BLOCKED 0x10u
#define THREAD_ZOMBIE 0x08u

/* The scheduler owns a fixed TCB table for now. Ready/sleep lists are
 * intrusive so no extra allocator or list nodes are needed. */
typedef struct nexus_tcb {
    uint64_t rsp;
    uint64_t stack_base;
    uint64_t stack_size;
    uint64_t id;
    uint64_t process_pid;
    uint64_t switches;
    uint64_t runtime_ticks;
    uint64_t wake_tick;
    uint64_t magic;
    uint32_t state;
    uint32_t flags;
    void (*entry)(void *arg);
    void *arg;
    struct nexus_tcb *next;
    struct nexus_tcb *wait_next;
} nexus_tcb_t;

static volatile uint64_t g_ticks;
static volatile uint64_t g_quantum_expirations;
static volatile uint64_t g_context_switches;
static volatile uint32_t g_quantum_ticks;
static volatile uint32_t g_quantum_remaining;
static volatile uint32_t g_timer_hz;
static volatile int g_reschedule_pending;
static volatile int g_wakeup_pending;
static volatile int g_ready;

static nexus_tcb_t g_tcbs[SCHED_MAX_THREADS];
static nexus_tcb_t *g_current;
static nexus_tcb_t *g_ready_head;
static nexus_tcb_t *g_ready_tail;
static nexus_tcb_t *g_sleep_head;
static uint32_t g_thread_count;
static uint32_t g_ready_count;
static uint32_t g_sleeping_count;
static uint64_t g_next_tid = 1;

extern void arch_context_switch(uint64_t *old_rsp, uint64_t new_rsp);

static uint32_t calculate_quantum_ticks(uint32_t timer_hz) {
    uint64_t ticks = ((uint64_t)timer_hz * NEXUS_SCHEDULER_DEFAULT_QUANTUM_MS) / 1000ULL;
    if (ticks == 0) ticks = 1;
    if (ticks > 0xFFFFFFFFULL) ticks = 0xFFFFFFFFULL;
    return (uint32_t)ticks;
}

static uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; pop %0" : "=r"(flags));
    __asm__ volatile ("cli" ::: "memory");
    return flags;
}

static void irq_restore(uint64_t flags) {
    if (flags & (1ULL << 9)) __asm__ volatile ("sti" ::: "memory");
}

static void ready_append(nexus_tcb_t *thread) {
    if (thread == NULL) return;
    thread->next = NULL;
    if (g_ready_tail == NULL) {
        g_ready_head = thread;
        g_ready_tail = thread;
    } else {
        g_ready_tail->next = thread;
        g_ready_tail = thread;
    }
    g_ready_count++;
}

static nexus_tcb_t *ready_pop(void) {
    nexus_tcb_t *thread = g_ready_head;
    if (thread == NULL) return NULL;

    g_ready_head = thread->next;
    if (g_ready_head == NULL) g_ready_tail = NULL;
    thread->next = NULL;
    if (g_ready_count > 0) g_ready_count--;
    return thread;
}

static void sleep_insert(nexus_tcb_t *thread) {
    if (thread == NULL) return;

    thread->next = NULL;
    if (g_sleep_head == NULL || thread->wake_tick < g_sleep_head->wake_tick) {
        thread->next = g_sleep_head;
        g_sleep_head = thread;
        g_sleeping_count++;
        return;
    }

    nexus_tcb_t *cursor = g_sleep_head;
    while (cursor->next != NULL && cursor->next->wake_tick <= thread->wake_tick) {
        cursor = cursor->next;
    }
    thread->next = cursor->next;
    cursor->next = thread;
    g_sleeping_count++;
}

static void wake_due_threads(void) {
    while (g_sleep_head != NULL && g_sleep_head->wake_tick <= g_ticks) {
        nexus_tcb_t *thread = g_sleep_head;
        g_sleep_head = thread->next;
        thread->next = NULL;
        if (g_sleeping_count > 0) g_sleeping_count--;
        thread->state = THREAD_READY;
        ready_append(thread);
    }
    g_wakeup_pending = (g_sleep_head != NULL && g_sleep_head->wake_tick <= g_ticks);
}

static void wake_thread_locked(nexus_tcb_t *thread) {
    if (thread == NULL || thread->state != THREAD_SLEEPING) return;

    nexus_tcb_t **link = &g_sleep_head;
    while (*link != NULL && *link != thread) link = &(*link)->next;
    if (*link != thread) return;

    *link = thread->next;
    thread->next = NULL;
    if (g_sleeping_count > 0) g_sleeping_count--;
    thread->wake_tick = 0;
    thread->state = THREAD_READY;
    ready_append(thread);
}

static void reap_zombies(void) {
    /* A zombie can only be reaped by a different thread, so its old stack is
     * no longer active when this function runs. TID 0 has no heap stack. */
    for (uint32_t i = 1; i < SCHED_MAX_THREADS; i++) {
        nexus_tcb_t *thread = &g_tcbs[i];
        if (thread->magic != THREAD_MAGIC || thread->state != THREAD_ZOMBIE) continue;

        if (thread->stack_base != 0) kfree((void *)(uintptr_t)thread->stack_base);
        thread->stack_base = 0;
        thread->stack_size = 0;
        thread->rsp = 0;
        thread->entry = NULL;
        thread->arg = NULL;
        thread->next = NULL;
        thread->wait_next = NULL;
        thread->wake_tick = 0;
        thread->magic = 0;
        thread->state = 0;
    }
}

static nexus_tcb_t *pick_next(void) {
    return ready_pop();
}

static void user_process_bootstrap(void *arg) {
    uint64_t pid = (uint64_t)(uintptr_t)arg;
    if (!usermode_enter(pid)) {
        (void)process_exit(pid);
    }
    thread_exit();
}

static void thread_bootstrap(void) {
    __asm__ volatile ("sti" ::: "memory");
    nexus_tcb_t *thread = g_current;
    if (thread != NULL && thread->entry != NULL) {
        thread->entry(thread->arg);
    }
    thread_exit();
    for (;;) __asm__ volatile ("cli; hlt");
}

static void init_thread_stack(nexus_tcb_t *thread) {
    uintptr_t top = (uintptr_t)thread->stack_base + thread->stack_size;
    top &= ~((uintptr_t)0xFULL);

    /* arch_context_switch restores: r15,r14,r13,r12,rbx,rbp,ret. */
    uint64_t *stack = (uint64_t *)(top - sizeof(uint64_t));
    *--stack = (uint64_t)(uintptr_t)thread_bootstrap; /* ret */
    *--stack = 0; /* rbp */
    *--stack = 0; /* rbx */
    *--stack = 0; /* r12 */
    *--stack = 0; /* r13 */
    *--stack = 0; /* r14 */
    *--stack = 0; /* r15 */
    thread->rsp = (uint64_t)(uintptr_t)stack;
}

static nexus_tcb_t *find_thread_by_id(uint64_t id) {
    for (uint32_t i = 0; i < SCHED_MAX_THREADS; i++) {
        if (g_tcbs[i].magic == THREAD_MAGIC && g_tcbs[i].id == id) return &g_tcbs[i];
    }
    return NULL;
}

void scheduler_wait_queue_init(nexus_wait_queue_t *queue) {
    if (!queue) return;
    queue->head = 0;
    queue->count = 0;
    for (uint32_t i = 0; i < NEXUS_SCHEDULER_MAX_THREADS; i++) queue->ids[i] = 0;
}

int scheduler_wait_queue_contains(const nexus_wait_queue_t *queue, uint64_t id) {
    if (!queue || id == 0) return 0;
    for (uint32_t i = 0; i < queue->count; i++) {
        uint32_t pos = (queue->head + i) % NEXUS_SCHEDULER_MAX_THREADS;
        if (queue->ids[pos] == id) return 1;
    }
    return 0;
}

void scheduler_wait_queue_push_locked(nexus_wait_queue_t *queue, uint64_t id) {
    if (!queue || id == 0 || queue->count >= NEXUS_SCHEDULER_MAX_THREADS ||
        scheduler_wait_queue_contains(queue, id)) return;
    uint32_t pos = (queue->head + queue->count) % NEXUS_SCHEDULER_MAX_THREADS;
    queue->ids[pos] = id;
    queue->count++;
}

uint64_t scheduler_wait_queue_pop_locked(nexus_wait_queue_t *queue) {
    if (!queue || queue->count == 0) return 0;
    uint64_t id = queue->ids[queue->head];
    queue->ids[queue->head] = 0;
    queue->head = (queue->head + 1) % NEXUS_SCHEDULER_MAX_THREADS;
    queue->count--;
    return id;
}

void scheduler_wake_thread_locked(uint64_t id) {
    nexus_tcb_t *thread = find_thread_by_id(id);
    if (!thread || thread->state != THREAD_BLOCKED) return;
    thread->state = THREAD_READY;
    ready_append(thread);
    g_wakeup_pending = 1;
    g_reschedule_pending = 1;
}

int thread_block_on_wait_queue(nexus_wait_queue_t *queue) {
    if (!g_ready || !queue || g_current == NULL || g_current->id == 0) return 0;
    uint64_t flags = irq_save();
    uint64_t tid = g_current->id;
    if (!scheduler_wait_queue_contains(queue, tid)) {
        scheduler_wait_queue_push_locked(queue, tid);
    }
    g_current->state = THREAD_BLOCKED;
    g_reschedule_pending = 1;
    irq_restore(flags);
    scheduler_process();
    return 1;
}

int scheduler_wake_thread(uint64_t id) {
    if (!g_ready || id == 0) return 0;
    uint64_t flags = irq_save();
    nexus_tcb_t *thread = find_thread_by_id(id);
    if (!thread || thread->state != THREAD_BLOCKED) {
        irq_restore(flags);
        return 0;
    }
    scheduler_wake_thread_locked(id);
    irq_restore(flags);
    return 1;
}

void scheduler_init(uint32_t timer_hz) {
    g_ticks = 0;
    g_quantum_expirations = 0;
    g_context_switches = 0;
    g_timer_hz = timer_hz ? timer_hz : 100;
    g_quantum_ticks = calculate_quantum_ticks(g_timer_hz);
    g_quantum_remaining = g_quantum_ticks;
    g_reschedule_pending = 0;
    g_wakeup_pending = 0;
    g_ready = 1;
    g_thread_count = 0;
    g_ready_count = 0;
    g_sleeping_count = 0;
    g_next_tid = 1;
    g_current = &g_tcbs[0];
    g_ready_head = NULL;
    g_ready_tail = NULL;
    g_sleep_head = NULL;

    for (uint32_t i = 0; i < SCHED_MAX_THREADS; i++) {
        g_tcbs[i].magic = 0;
        g_tcbs[i].state = 0;
        g_tcbs[i].next = NULL;
        g_tcbs[i].wait_next = NULL;
        g_tcbs[i].wake_tick = 0;
    }

    uint64_t rsp;
    __asm__ volatile ("mov %%rsp, %0" : "=r"(rsp));
    g_current->magic = THREAD_MAGIC;
    g_current->id = 0;
    g_current->process_pid = 0;
    g_current->rsp = rsp;
    g_current->stack_base = 0;
    g_current->stack_size = 0;
    g_current->switches = 0;
    g_current->runtime_ticks = 0;
    g_current->wake_tick = 0;
    g_current->state = THREAD_RUNNING;
    g_current->entry = NULL;
    g_current->arg = NULL;
    g_current->next = NULL;
    g_current->wait_next = NULL;
    g_thread_count = 1;
}

void scheduler_tick_irq(void) {
    if (!g_ready) return;
    g_ticks++;
    if (g_current != NULL) g_current->runtime_ticks++;
    if (g_quantum_remaining > 0) g_quantum_remaining--;
    if (g_quantum_remaining == 0) {
        g_quantum_expirations++;
        g_reschedule_pending = 1;
        g_quantum_remaining = g_quantum_ticks;
    }

    /* Do not mutate scheduler lists from IRQ context. The normal-context
     * scheduler consumes this hint and performs the actual wakeup. */
    if (g_sleep_head != NULL && g_sleep_head->wake_tick <= g_ticks) {
        g_wakeup_pending = 1;
        g_reschedule_pending = 1;
    }
}

void scheduler_request_reschedule(void) {
    if (g_ready) g_reschedule_pending = 1;
}

void scheduler_process(void) {
    if (!g_ready) return;

    uint64_t flags = irq_save();
    wake_due_threads();
    reap_zombies();
    int should_schedule = g_reschedule_pending || g_wakeup_pending;
    if (!should_schedule) {
        irq_restore(flags);
        return;
    }
    g_reschedule_pending = 0;
    g_wakeup_pending = 0;

    nexus_tcb_t *old = g_current;
    if (old == NULL) {
        irq_restore(flags);
        return;
    }

    if (old->state == THREAD_RUNNING) {
        old->state = THREAD_READY;
        ready_append(old);
    }

    nexus_tcb_t *next = pick_next();
    if (next == NULL) {
        /* Keep the current execution context runnable if there is nobody else
         * to run. This is the idle-safe path for the fixed scheduler. */
        old->state = THREAD_RUNNING;
        irq_restore(flags);
        return;
    }

    if (next == old) {
        old->state = THREAD_RUNNING;
        irq_restore(flags);
        return;
    }

    next->state = THREAD_RUNNING;
    g_current = next;

    /* Address-space ownership follows the scheduler TCB. Kernel threads use
     * the permanent kernel CR3; user threads use their private process CR3.
     * Switch CR3 before handing over the kernel stack so a resumed thread
     * never executes under another process's address space. */
    uint64_t next_cr3 = paging_kernel_cr3();
    if (next->process_pid != 0) {
        nexus_process_t *np = process_get(next->process_pid);
        if (np && np->state != PROCESS_EXITED && np->address_space_cr3 != 0) {
            next_cr3 = np->address_space_cr3;
            process_set_current(np->pid);
        } else {
            next->state = THREAD_ZOMBIE;
            g_current = old;
            old->state = THREAD_RUNNING;
            irq_restore(flags);
            return;
        }
    } else {
        process_clear_current();
    }
    (void)paging_switch_cr3(next_cr3);

    old->switches++;
    next->switches++;
    g_context_switches++;

    __asm__ volatile ("cli" ::: "memory");
    /* Keep interrupts disabled across the actual stack hand-off. The resumed
     * thread returns here with its own saved scheduler context and enables
     * interrupts at the normal-context boundary. */
    arch_context_switch(&old->rsp, next->rsp);
    __asm__ volatile ("sti" ::: "memory");
}

uint64_t thread_create(void (*entry)(void *), void *arg) {
    if (!g_ready || entry == NULL || !heap_is_ready()) return 0;

    uint64_t flags = irq_save();
    nexus_tcb_t *slot = NULL;
    for (uint32_t i = 1; i < SCHED_MAX_THREADS; i++) {
        if (g_tcbs[i].magic == 0) { slot = &g_tcbs[i]; break; }
    }
    if (slot == NULL) {
        irq_restore(flags);
        return 0;
    }

    void *stack = kmalloc(SCHED_STACK_SIZE);
    if (stack == NULL) {
        irq_restore(flags);
        return 0;
    }

    slot->magic = THREAD_MAGIC;
    slot->id = g_next_tid++;
    slot->process_pid = 0;
    slot->stack_base = (uint64_t)(uintptr_t)stack;
    slot->stack_size = SCHED_STACK_SIZE;
    slot->switches = 0;
    slot->runtime_ticks = 0;
    slot->wake_tick = 0;
    slot->state = THREAD_READY;
    slot->entry = entry;
    slot->arg = arg;
    slot->next = NULL;
    slot->wait_next = NULL;
    init_thread_stack(slot);
    ready_append(slot);
    g_thread_count++;

    uint64_t id = slot->id;
    irq_restore(flags);
    return id;
}

uint64_t thread_create_user_process(uint64_t pid) {
    if (!g_ready || pid == 0 || !heap_is_ready() || !usermode_ready()) return 0;

    nexus_process_t *process = process_get(pid);
    if (!process || process->state == PROCESS_EXITED || process->scheduler_thread_id != 0) return 0;

    uint64_t tid = thread_create(user_process_bootstrap, (void *)(uintptr_t)pid);
    if (tid == 0) return 0;

    /* thread_create owns the kernel stack; expose its top through the TCB so
     * CPL3 -> CPL0 gates can use the same stack via TSS.RSP0. */
    for (uint32_t i = 1; i < SCHED_MAX_THREADS; ++i) {
        if (g_tcbs[i].magic == THREAD_MAGIC && g_tcbs[i].id == tid) {
            g_tcbs[i].process_pid = pid;
            process->kernel_stack = g_tcbs[i].stack_base + g_tcbs[i].stack_size;
            process->scheduler_thread_id = tid;
            process->flags |= PROCESS_FLAG_SCHEDULER_OWNED;
            return tid;
        }
    }

    return 0;
}

void thread_yield(void) {
    if (!g_ready) return;
    scheduler_request_reschedule();
    scheduler_process();
}

void thread_sleep_ms(uint64_t milliseconds) {
    if (!g_ready || g_current == NULL || g_current->id == 0) return;

    uint64_t ticks = (milliseconds * (uint64_t)g_timer_hz + 999ULL) / 1000ULL;
    if (ticks == 0) ticks = 1;

    uint64_t flags = irq_save();
    g_current->wake_tick = g_ticks + ticks;
    g_current->state = THREAD_SLEEPING;
    sleep_insert(g_current);
    g_reschedule_pending = 1;
    irq_restore(flags);
    scheduler_process();
}

int thread_wakeup(uint64_t id) {
    if (!g_ready) return 0;

    uint64_t flags = irq_save();
    nexus_tcb_t *thread = NULL;
    for (uint32_t i = 0; i < SCHED_MAX_THREADS; i++) {
        if (g_tcbs[i].magic == THREAD_MAGIC && g_tcbs[i].id == id) {
            thread = &g_tcbs[i];
            break;
        }
    }
    if (thread == NULL || thread->state != THREAD_SLEEPING) {
        irq_restore(flags);
        return 0;
    }

    wake_thread_locked(thread);
    g_wakeup_pending = 1;
    g_reschedule_pending = 1;
    irq_restore(flags);
    return 1;
}

void thread_exit(void) {
    if (!g_ready || g_current == NULL || g_current->id == 0) return;
    g_current->state = THREAD_ZOMBIE;
    if (g_thread_count > 0) g_thread_count--;
    g_reschedule_pending = 1;
    scheduler_process();
    for (;;) __asm__ volatile ("cli; hlt");
}

int scheduler_is_ready(void) { return g_ready != 0; }
int scheduler_reschedule_pending(void) { return g_reschedule_pending != 0; }
uint64_t scheduler_ticks(void) { return g_ticks; }
uint64_t scheduler_elapsed_ms(void) { return g_timer_hz ? (g_ticks * 1000ULL) / g_timer_hz : 0; }
uint64_t scheduler_quantum_expirations(void) { return g_quantum_expirations; }
uint64_t scheduler_context_switches(void) { return g_context_switches; }
uint32_t scheduler_timer_hz(void) { return g_timer_hz; }
uint32_t scheduler_quantum_ticks(void) { return g_quantum_ticks; }
uint32_t scheduler_thread_count(void) { return g_thread_count; }
uint32_t scheduler_max_threads(void) { return SCHED_MAX_THREADS; }
uint32_t scheduler_ready_count(void) { return g_ready_count; }
uint32_t scheduler_sleeping_count(void) { return g_sleeping_count; }
uint64_t scheduler_current_thread_id(void) { return g_current ? g_current->id : 0; }
uint64_t scheduler_thread_switches(uint64_t id) {
    for (uint32_t i = 0; i < SCHED_MAX_THREADS; i++) if (g_tcbs[i].magic == THREAD_MAGIC && g_tcbs[i].id == id) return g_tcbs[i].switches;
    return 0;
}
uint64_t scheduler_thread_runtime_ticks(uint64_t id) {
    for (uint32_t i = 0; i < SCHED_MAX_THREADS; i++) if (g_tcbs[i].magic == THREAD_MAGIC && g_tcbs[i].id == id) return g_tcbs[i].runtime_ticks;
    return 0;
}
