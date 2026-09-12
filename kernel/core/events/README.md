# NexusOS Kernel Event Queue & Wait Integration

The event subsystem is split into two layers:

1. **IRQ event queue** — IRQ handlers capture minimal hardware state into the
   fixed 256-entry ring.
2. **Kernel event notification** — `kernel_events_process()` consumes records
   in normal context, advances a per-event-type sequence counter and wakes
   kernel threads waiting for that event class.

## Waiting

A kernel thread can call:

```c
if (kernel_events_wait(NEXUS_EVENT_KEYBOARD_SCANCODE)) {
    /* A keyboard event was processed. */
}
```

Waiting threads are put into scheduler wait queues and become `THREAD_BLOCKED`;
they do not busy-loop. The event record itself remains owned by the normal
kernel event consumer, so waiting is notification rather than a second queue
consumer.

Wakeups are performed from normal kernel context. IRQ handlers never manipulate
scheduler lists or perform a context switch.

Sequence counters make the wait edge-safe: an event processed before a thread
actually blocks is detected and the thread does not sleep unnecessarily.
