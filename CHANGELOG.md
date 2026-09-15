## 0.5.18 — Enstein — Private process address spaces

- Added per-process private CR3/page-table roots.
- Cloned the kernel paging hierarchy into each process address space.
- Added CR3-aware map/unmap/translation APIs.
- Added safe 2 MiB-to-4 KiB page-table splitting when a user page falls inside the identity map.
- Scheduler now switches CR3 together with the process-owned TCB.
- ELF loading now initializes user memory through process address translations instead of assuming the active kernel CR3 contains user mappings.
- Preserved the existing identity-mapped kernel/hardware address space and deferred higher-half work.

## 0.5.17 — Enstein — Syscall entry/return foundation

- Added DPL3 `INT 0x80` syscall gate.
- Added x86_64 syscall entry/return assembly preserving all GPRs.
- Added user return-frame validation.
- Enabled NOP and GETPID through the user syscall ABI.
- Kept EXIT deferred until scheduler-owned user process termination exists.

# NexusOS Changelog

- Added `Files` manifest field for multiple package payloads.
- Added strict 8.3 payload-name validation.
- Package installation preflights every payload before mutation.
- Multiple payloads are staged and rolled back in reverse order on failure.
- `manifest.nxm` remains the final publication/commit point.

# Changelog

## 0.5.15 - Enstein

- Added ELF64 x86_64 user-space loader foundation.
- Added PT_LOAD validation, image loading and BSS zero-fill.
- Added per-page user/writable/NX mapping flags.
- Added fixed user stack mapping and process-owned page tracking.
- Added `elf-run <path>` synchronous Ring-3 execution path.
- Kept private CR3, syscall entry/return and scheduler-owned user processes for later milestones.


## [0.5.13] — Package Installation Transaction

- Added transactional package installation foundation.
- Added manifest validation before filesystem mutation.
- Added staged application directory creation.
- Added bounded Entry payload installation.
- Made `manifest.nxm` the final publication/commit point.
- Added rollback for newly created payload/directory on failure.
- Added immediate application rediscovery after successful installation.

## [0.5.12] — Writable FAT32 Foundation

NexusOS 0.5.12 adds the first bounded write path to the real FAT32/AHCI storage backend.

- Added AHCI `WRITE DMA EXT` sector writes with the existing polling model.
- Added FAT32 cluster allocation/freeing and mirrored FAT updates.
- Added creation/overwrite of short 8.3 files.
- Added creation of short 8.3 directories with `.` / `..` entries.
- Added writable FAT32 routing for VFS `mkdir`, `touch`, and `write` when the mount is not read-only.
- Switched the active `/mnt/disk0` FAT32 mount to read-write mode.
- Kept LFN, permissions, journaling, transactions and executable loading out of this milestone.
- Updated active version to `0.5.12 - Enstein`.

## [0.5.11] — Application Discovery

- Added bounded FAT32 directory enumeration for non-printing discovery.
- Added read-only `/system/apps` and `/userdata/apps` manifest discovery.
- Valid `manifest.nxm` files are registered with the App Manager using source classification.
- Kept package installation, ELF loading and userspace execution out of this milestone.
- Updated active version to `0.5.11 - Enstein`.

## [0.5.10] — App Manager Foundation

- Added a centralized application registry with stable application IDs.
- Added explicit application source classification for built-in, system and userdata apps.
- Added built-in registration helper used by NexusOS kernel initialization.
- Added per-application lifecycle state and launch counters.
- Added active-application tracking and explicit stop-by-ID support.
- Kept binary execution out of the manager; ELF loading and userspace execution remain future work.
- Updated active version to `0.5.10 - Enstein`.

## [0.5.4] — Processes & Userspace Foundation
- Consolidated process lifecycle, stable PIDs and per-process execution metadata.
- Added user virtual-range reservation and PMM-backed 4 KiB U/S mappings with cleanup.
- Added x86_64 TSS.RSP0 and IRETQ Ring-3 transition foundation.
- Added kernel-side syscall dispatch foundation (NOP, GETPID, EXIT) without enabling an unsafe entry path.
- Documented remaining limitations: private CR3, full address-space isolation, ELF loading and runnable user programs remain future work.

## [0.5.4.3] — User/Kernel Memory Permissions & Address Space Activation
- Added PMM-backed 4 KiB user mappings with U/S permissions.
- Added per-process mapping ownership and cleanup.
- Recorded active CR3 explicitly; private CR3 remains future work.

## [0.5.4.2] — TSS & Ring-3 Transition Foundation

- Completed the x86_64 GDT user selectors and TSS transition contract as an explicit userspace subsystem API.
- Added architectural IRETQ ring-3 transition trampoline with user CS/SS selectors.
- Added per-process user execution context metadata and TSS.RSP0 hand-off.
- Kept page mapping, CR3 switching, ELF loading and syscalls out of this block.

## [0.5.4.1] — Process & Address Space Foundation

- Added fixed-size kernel process table with stable PIDs and lifecycle states.
- Added current-process metadata and IRQ-safe process-table locking.
- Added reserved user virtual-range metadata for future page mapping.
- Added explicit future CR3 ownership field without switching address spaces yet.
- Integrated process foundation into kernel startup.

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

## [0.5.7] — GUI / Window System 2.0

- Added native bounded window abstraction and manager.
- Added window title, bounds, visibility and focus state.
- Added active-window tracking and close events.
- Added mouse title-bar dragging with screen bounds.
- Integrated Files, Terminal, Settings and Search with shared window geometry.
- Preserved the existing desktop renderer and deferred input architecture.

## [0.5.6] — Unified Input & USB HID

- Added unified input accounting for PS/2 and USB HID sources.
- Added xHCI HID boot mouse discovery and interrupt-IN report processing.
- Kept existing xHCI boot keyboard path and normal-context polling model.
- Added USB mouse reports to the existing GUI coordinate/button/wheel state.
- Updated `inputinfo` with PS/2/USB device state and event counters.
- IRQ handlers remain capture-only; HID polling and input processing stay outside IRQ context.

## [0.5.5] — Shell & I/O Foundation
- Added robust in-place shell parser with quoting and backslash escapes.
- Added command sequencing (`;`) and conditional sequencing (`&&`).
- Added parser-level recognition for pipelines and redirections, keeping unsupported execution explicit.
