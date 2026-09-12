# NexusOS Scheduler

## 0.5.3.6 — Ready Queue, Sleep/Wakeup & Event Blocking

This milestone replaces the TID scan from 0.5.3.3 with intrusive scheduler
queues and adds kernel-thread sleeping and explicit wakeup.

### Ready queue

The scheduler keeps a FIFO ready queue of runnable threads. A thread that
creates/yields or reaches the end of its quantum is appended to the tail, and
the next runnable thread is removed from the head. TID ordering is no longer
used for scheduling decisions.

### Sleep queue

Sleeping threads are kept in a wake-time ordered intrusive list. The public
API is:

```c
void thread_sleep_ms(uint64_t milliseconds);
int thread_wakeup(uint64_t id);
```

`thread_sleep_ms()` converts milliseconds to PIT ticks, marks the current
thread SLEEPING and enters the normal scheduler boundary. `thread_wakeup()`
removes a sleeping thread from the sleep list and places it at the ready-queue
tail.

Timer IRQ0 only increments the scheduler clock and sets a deferred wakeup
hint. It never mutates the ready/sleep lists and never switches stacks.
Expired sleepers are moved to the ready queue from `scheduler_process()`.

### Thread lifecycle

TCB states are now:

- `READY`
- `RUNNING`
- `SLEEPING`
- `ZOMBIE`

Finished kernel threads remain zombies until another thread reaches the
scheduler boundary, where their heap-backed stacks and TCB slots are safely
reclaimed.

### Diagnostics

The scheduler exposes ready/sleeping counts in addition to the existing tick,
quantum and context-switch counters. This keeps `meminfo` useful without
walking internal scheduler lists.
