#include "event_queue.h"

/* Single logical producer: x86 interrupt handlers are serialized because IF
 * is cleared on entry to an interrupt gate. Single consumer: kernel main loop. */
static nexus_event_t g_queue[NEXUS_EVENT_QUEUE_CAPACITY];
static volatile uint32_t g_head;
static volatile uint32_t g_tail;
static volatile uint64_t g_pushed;
static volatile uint64_t g_dropped;
static volatile uint64_t g_popped;
static volatile int g_ready;

static inline void event_release(void) {
    __asm__ volatile ("" ::: "memory");
}

static inline void event_acquire(void) {
    __asm__ volatile ("" ::: "memory");
}

void event_queue_init(void) {
    g_head = 0;
    g_tail = 0;
    g_pushed = 0;
    g_dropped = 0;
    g_popped = 0;
    g_ready = 1;
}

int event_queue_push(nexus_event_type_t type, uint32_t source, uint64_t data) {
    if (!g_ready || type == NEXUS_EVENT_NONE) return 0;

    /* The current IDT path re-enables IF before dispatching the device, so a
     * timer IRQ can nest a keyboard/mouse IRQ. Serialize the tiny producer
     * critical section while preserving the caller's IF state. */
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");

    uint32_t head = g_head;
    uint32_t next = (head + 1u) % NEXUS_EVENT_QUEUE_CAPACITY;
    uint32_t tail = g_tail;

    if (next == tail) {
        g_dropped++;
        __asm__ volatile ("pushq %0; popfq" : : "r"(flags) : "memory", "cc");
        return 0;
    }

    g_queue[head].type = (uint32_t)type;
    g_queue[head].source = source;
    g_queue[head].data = data;
    event_release();
    g_head = next;
    g_pushed++;
    __asm__ volatile ("pushq %0; popfq" : : "r"(flags) : "memory", "cc");
    return 1;
}

int event_queue_pop(nexus_event_t *event) {
    if (!g_ready || event == 0) return 0;

    uint32_t tail = g_tail;
    uint32_t head = g_head;
    if (tail == head) return 0;

    event_acquire();
    *event = g_queue[tail];
    g_tail = (tail + 1u) % NEXUS_EVENT_QUEUE_CAPACITY;
    g_popped++;
    return 1;
}

uint32_t event_queue_count(void) {
    uint32_t head = g_head;
    uint32_t tail = g_tail;
    if (head >= tail) return head - tail;
    return NEXUS_EVENT_QUEUE_CAPACITY - tail + head;
}

uint64_t event_queue_pushed(void) { return g_pushed; }
uint64_t event_queue_dropped(void) { return g_dropped; }
uint64_t event_queue_popped(void) { return g_popped; }
int event_queue_is_ready(void) { return g_ready != 0; }
