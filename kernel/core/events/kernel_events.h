#ifndef NEXUSOS_KERNEL_EVENTS_H
#define NEXUSOS_KERNEL_EVENTS_H

#include <stdint.h>
#include "event_queue.h"

/*
 * Process-safe event notification layer. The event ring remains the single
 * source of event records; this layer adds per-type sequence counters and
 * wait queues so kernel threads can sleep until a matching event has been
 * processed by normal kernel context.
 */
void kernel_events_init(void);
void kernel_events_process(void);
uint64_t kernel_events_processed(void);

/* Block the calling kernel thread until a new event of `type` is processed.
 * Returns 1 after wakeup, 0 when called from unsupported/early context. */
int kernel_events_wait(nexus_event_type_t type);

/* Monotonic processed-event counters. */
uint64_t kernel_events_sequence(nexus_event_type_t type);
uint32_t kernel_events_waiter_count(nexus_event_type_t type);

#endif
