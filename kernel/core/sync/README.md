# Kernel synchronization — NexusOS 0.5.3.5

This module provides the first blocking synchronization primitives for the
kernel scheduler:

- `nexus_spinlock_t` — short, non-sleeping atomic lock.
- `nexus_mutex_t` — ownership-aware mutex with FIFO waiters.
- Scheduler wait queues — fixed-size, allocation-free queues of blocked TIDs.

## Rules

Spinlocks may be used in interrupt-sensitive code. `spinlock_lock_irqsave()`
keeps interrupts disabled for the critical section.

Mutexes may sleep and therefore must not be acquired from IRQ context. A
contending thread is moved to `THREAD_BLOCKED`, the scheduler switches to a
ready thread, and `mutex_unlock()` transfers ownership directly to the first
waiter before waking it.

The current implementation uses a fixed eight-thread limit matching the
0.5.x kernel scheduler. No dynamic wait-queue allocation is performed.
