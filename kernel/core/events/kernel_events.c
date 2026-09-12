#include "kernel_events.h"
#include "event_queue.h"
#include "pit.h"
#include "keyboard.h"
#include "mouse.h"
#include "xhci.h"
#include "scheduler.h"

#define KERNEL_EVENT_TYPE_COUNT 4u

static volatile uint64_t g_processed;
static volatile uint64_t g_sequence[KERNEL_EVENT_TYPE_COUNT];
static nexus_wait_queue_t g_waiters[KERNEL_EVENT_TYPE_COUNT];

static uint64_t irq_save_events(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void irq_restore_events(uint64_t flags) {
    if (flags & (1ULL << 9)) __asm__ volatile ("sti" ::: "memory");
}

static int valid_type(nexus_event_type_t type) {
    return (uint32_t)type < KERNEL_EVENT_TYPE_COUNT && type != NEXUS_EVENT_NONE;
}

static void wake_event_waiters_locked(nexus_event_type_t type) {
    if (!valid_type(type)) return;

    /* Broadcast semantics: every thread waiting for this event class gets a
     * chance to observe the new sequence. This is a notification primitive,
     * not a second consumer queue, so the actual event record stays owned by
     * kernel_events_process(). */
    for (;;) {
        uint64_t tid = scheduler_wait_queue_pop_locked(&g_waiters[(uint32_t)type]);
        if (tid == 0) break;
        scheduler_wake_thread_locked(tid);
    }
}

void kernel_events_init(void) {
    g_processed = 0;
    for (uint32_t i = 0; i < KERNEL_EVENT_TYPE_COUNT; i++) {
        g_sequence[i] = 0;
        scheduler_wait_queue_init(&g_waiters[i]);
    }
    event_queue_init();
}

void kernel_events_process(void) {
    nexus_event_t event;
    while (event_queue_pop(&event)) {
        switch ((nexus_event_type_t)event.type) {
            case NEXUS_EVENT_TIMER_TICK:
                /* PIT and scheduler accounting happened in IRQ context; the
                 * scheduler hand-off and xHCI polling happen here. */
                scheduler_process();
                xhci_poll();
                break;
            case NEXUS_EVENT_KEYBOARD_SCANCODE:
                keyboard_process_scancode((uint8_t)event.data);
                break;
            case NEXUS_EVENT_MOUSE_BYTE:
                mouse_process_byte((uint8_t)event.data);
                break;
            default:
                break;
        }

        uint32_t type = event.type;
        if (type < KERNEL_EVENT_TYPE_COUNT && type != NEXUS_EVENT_NONE) {
            uint64_t flags = irq_save_events();
            g_sequence[type]++;
            wake_event_waiters_locked((nexus_event_type_t)type);
            irq_restore_events(flags);
        }
        g_processed++;
    }
}

int kernel_events_wait(nexus_event_type_t type) {
    if (!valid_type(type) || !scheduler_is_ready() ||
        scheduler_current_thread_id() == 0) return 0;

    uint32_t index = (uint32_t)type;
    for (;;) {
        uint64_t before = kernel_events_sequence(type);
        uint64_t flags = irq_save_events();

        /* If normal context already processed a matching event since the last
         * observation, do not sleep. This closes the check/enqueue race. */
        if (g_sequence[index] != before) {
            irq_restore_events(flags);
            return 1;
        }

        uint64_t tid = scheduler_current_thread_id();
        scheduler_wait_queue_push_locked(&g_waiters[index], tid);
        irq_restore_events(flags);

        if (!thread_block_on_wait_queue(&g_waiters[index])) return 0;

        if (kernel_events_sequence(type) != before) return 1;
    }
}

uint64_t kernel_events_processed(void) { return g_processed; }

uint64_t kernel_events_sequence(nexus_event_type_t type) {
    if (!valid_type(type)) return 0;
    return g_sequence[(uint32_t)type];
}

uint32_t kernel_events_waiter_count(nexus_event_type_t type) {
    if (!valid_type(type)) return 0;
    uint64_t flags = irq_save_events();
    uint32_t count = g_waiters[(uint32_t)type].count;
    irq_restore_events(flags);
    return count;
}
