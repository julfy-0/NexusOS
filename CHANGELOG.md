## [0.5.3.6] — Process-safe Event Integration

- Added per-event-type sequence counters to the kernel event processor.
- Added blocking `kernel_events_wait()` for kernel threads.
- Added event-specific scheduler wait queues with broadcast wakeup.
- Closed the check/enqueue race so already-processed events do not cause unnecessary sleeps.
- Kept IRQ handlers capture-only: event wakeups and scheduler list changes stay in normal kernel context.
- Added event sequence/waiter diagnostics to `meminfo`.
- Updated the scheduler service thread to demonstrate event-driven blocking on keyboard events.
- Bumped active runtime version to `0.5.3.6 - Enstein`.

## [0.5.3.5] — Synchronization Primitives

- Added kernel spinlocks with atomic acquire, try-lock, release and IRQ-save helpers.
- Added ownership-aware mutexes with FIFO blocking waiters.
- Added fixed-size scheduler wait queues and `THREAD_BLOCKED`.
- Added blocking/wakeup scheduler APIs without performing context switches from IRQ context.
- Mutex unlock transfers ownership directly to the first waiter before waking it.
- Bumped active runtime version to `0.5.3.5 - Enstein`.

## [0.5.3.4] — Threads & TCB

- Added kernel thread control blocks (TCBs) with per-thread state, stack, ID, runtime and switch counters.
- Added fixed-size kernel thread slots and 16 KiB per-thread stacks allocated from the kernel heap.
- Added x86_64 callee-saved context switching (`arch_context_switch`).
- Added thread bootstrap, `thread_create()`, `thread_yield()` and `thread_exit()`.
- Scheduler now performs round-robin thread selection at the normal-context reschedule boundary; IRQ handlers still never switch stacks.
- Added thread count/current TID information to `meminfo`.
- Bumped active runtime version to `0.5.3.4 - Enstein`.

## [0.5.3.2] — Timer-driven Scheduler Foundation

- Added `kernel/core/scheduler/` with scheduler clock and quantum accounting.
- Added PIT-driven scheduler ticks at the existing 100 Hz timer rate.
- Added a 100 ms default scheduling quantum and deferred reschedule requests.
- Added a normal-kernel-context scheduler hand-off after timer events.
- Kept register/stack context switching out of IRQ context; TCB/context switching remains the next milestone.
- Added scheduler diagnostics APIs for ticks, elapsed time, quantum expirations and context switches.
- Bumped runtime version to `0.5.3.2 - Enstein`.

## [0.5.3.1] — Interrupt/Event Queue

- Added a fixed-size 256-event kernel ring queue.
- Converted PIT, PS/2 keyboard and PS/2 mouse IRQ paths to capture-only handlers.
- Deferred keyboard scancode decoding and mouse packet assembly to normal kernel context.
- Moved xHCI polling out of IRQ0 into the kernel event loop.
- Removed shell/GUI execution from PS/2 IRQ context.
- Added queue push/pop/drop statistics for diagnostics.
- Bumped runtime version to `0.5.3.1 - Enstein`.
