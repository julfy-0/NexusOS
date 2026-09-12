#ifndef NEXUSOS_EVENT_QUEUE_H
#define NEXUSOS_EVENT_QUEUE_H

#include <stdint.h>

/*
 * NexusOS 0.5.3.6 — Interrupt/Event Queue API.
 *
 * IRQ handlers are producers and the kernel main loop is the consumer.
 * Interrupt handlers must only capture the minimum hardware state and enqueue
 * an event; driver logic and shell/GUI work execute later in normal kernel
 * context with interrupts enabled.
 */
typedef enum {
    NEXUS_EVENT_NONE = 0,
    NEXUS_EVENT_TIMER_TICK,
    NEXUS_EVENT_KEYBOARD_SCANCODE,
    NEXUS_EVENT_MOUSE_BYTE,
} nexus_event_type_t;

typedef struct {
    uint32_t type;
    uint32_t source;
    uint64_t data;
} nexus_event_t;

#define NEXUS_EVENT_SOURCE_TIMER    0u
#define NEXUS_EVENT_SOURCE_KEYBOARD 1u
#define NEXUS_EVENT_SOURCE_MOUSE    2u

#define NEXUS_EVENT_QUEUE_CAPACITY 256u

void event_queue_init(void);
int event_queue_push(nexus_event_type_t type, uint32_t source, uint64_t data);
int event_queue_pop(nexus_event_t *event);
uint32_t event_queue_count(void);
uint64_t event_queue_pushed(void);
uint64_t event_queue_dropped(void);
uint64_t event_queue_popped(void);
int event_queue_is_ready(void);

#endif
