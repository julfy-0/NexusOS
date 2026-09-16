## NexusOS 0.5.37 — Shell Identity and Hardware Hostname

### Added
- Root/user-aware shell prompt format.
- Motherboard manufacturer detection from SMBIOS/DMI Baseboard Information.
- Hardware-derived shell hostname.

### Shell
- Kernel shell prompt now uses `root@<manufacturer>` or the active user identity.
- The motherboard manufacturer is used as the hostname.
- Existing shell command history and terminal cursor behavior are preserved.

### Hardware
- SMBIOS Type 2 Baseboard Information is parsed by the UEFI loader.
- System manufacturer remains the fallback when baseboard manufacturer data is unavailable.
- Firmware-owned pointers are still copied before `ExitBootServices()`.

### Notes
- Colored startup logs from NexusOS 0.5.35 are preserved.
- USB stability changes from NexusOS 0.5.36 are preserved.
- Higher-half kernel work remains intentionally deferred to preserve boot stability.

## NexusOS 0.5.36 — USB Controller Stability and Keyboard Safety

### Added
- Safer xHCI BIOS ownership handoff.
- xHCI controller-not-ready and start-state checks.
- Bounded xHCI event polling budget.
- Clear USB initialization failure reasons.

### USB
- xHCI initialization now aborts cleanly when firmware ownership cannot be transferred.
- Host controller reset waits for Controller Not Ready (CNR) to clear.
- xHCI startup failure no longer proceeds into an invalid controller state.
- Multiple detected xHCI controllers continue to be tried until one initializes successfully.

### Input
- A broken USB controller can no longer monopolize normal-context event processing indefinitely.
- PS/2 keyboard remains available when xHCI initialization fails.
- USB HID polling is bounded per event-processing pass.

### Notes
- Multi-controller xHCI support from NexusOS 0.5.29 is preserved.
- Driver module startup logging and colored status output remain unchanged.
- Higher-half kernel work remains intentionally deferred to preserve boot stability.

## NexusOS 0.5.35 — Colored Core and Service Startup Logs

### Added
- Colored startup status output for core services and driver modules.
- Unified component startup format with name, type, version and status.
- Core initialization entries for GDT, IDT, kernel address space, physical memory manager, virtual memory manager, virtual arena, allocator, event queue and scheduler.

### Boot Logs
- Component names are highlighted in cyan.
- Successful initialization uses green `[ OK  ]`.
- Failed initialization uses red `[ FAILED ]`.
- Service and driver types remain visible next to their versions.
- Boot output stays compact without verbose subsystem diagnostics.

### Core Services
- GDT and IDT startup now use the same status format as driver modules.
- Kernel address space initialization is reported as a service.
- Physical memory manager reports failure when no usable memory map is available.
- Virtual memory manager, virtual arena, allocator, event queue and scheduler report independent startup status.

### Notes
- NexusOS 0.5.34 concise startup logging is preserved and extended with color.
- Driver module loading from NexusOS 0.5.30 remains unchanged.
- Multi-controller xHCI support from NexusOS 0.5.29 remains unchanged.
- Higher-half kernel work remains intentionally deferred to preserve boot stability.

## NexusOS 0.5.34 — Concise Driver and Service Startup Logs

### Added
- Compact startup status lines for drivers and services.
- Module name, type, version and initialization result in one line.

### Boot Logs
- Successful components use `[ OK  ]`.
- Failed components use `[ FAILED ]`.
- Driver and service versions are shown directly in the startup line.
- Redundant hardware/module diagnostics are removed from the normal boot path.

### Module Manager
- Existing priority-based module loading is preserved.
- Driver module version information is displayed at load time.
- Driver and service startup uses a unified status format.

### Notes
- Driver module architecture from NexusOS 0.5.30 is preserved.
- Multi-controller xHCI support from NexusOS 0.5.29 is preserved.
- Terminal scrollback shortcuts from NexusOS 0.5.33 are preserved.
- Higher-half kernel work remains intentionally deferred to preserve boot stability.

## NexusOS 0.5.33 — Terminal Scrollback Shortcuts

### Added
- Ctrl+Arrow terminal scrollback shortcuts.
- One-line scrollback navigation with Ctrl+Up and Ctrl+Down.

### Terminal
- Ctrl+Up scrolls the terminal one line toward older output.
- Ctrl+Down scrolls the terminal one line toward newer output.
- Existing Up/Down shell history navigation remains unchanged when Ctrl is not held.
- PageUp/PageDown continue to provide direct scrollback navigation.

### Input
- Added Ctrl modifier handling for PS/2 arrow navigation.
- Added Ctrl modifier handling for USB HID keyboard reports.
- Ctrl+Arrow shortcuts are ignored by the shell history handler.

### Notes
- Terminal text cursor from NexusOS 0.5.32 is preserved.
- Driver module load status from NexusOS 0.5.31 is preserved.
- Multi-controller xHCI support from NexusOS 0.5.29 is preserved.
- Higher-half kernel work remains intentionally deferred to preserve boot stability.

## NexusOS 0.5.32 — Terminal Text Cursor

### Added
- Blinking terminal text cursor for the NexusOS command line.
- Cursor visibility control for shell and graphical desktop transitions.
- PIT-driven cursor blink timing.

### Terminal
- Cursor is rendered at the active shell input position.
- Cursor is hidden while the input line is being edited or command output is rendered.
- Cursor is restored after shell input and history navigation.
- Cursor is automatically removed while the graphical desktop is active.

### Notes
- Driver module logging from NexusOS 0.5.31 is preserved.
- Multi-controller xHCI support from NexusOS 0.5.29 is preserved.
- Higher-half kernel work remains intentionally deferred to preserve boot stability.

## NexusOS 0.5.31 — Driver Module Load Status

### Added
- Per-module startup status lines for built-in kernel drivers.
- Clear `[ OK ]` and `[ FAILED ]` load results.

### Driver Logs
- Driver initialization now prints entries such as `USB driver module [ OK ]`.
- Each registered driver module reports its result immediately after initialization.
- Failed modules are reported explicitly instead of only contributing to the final summary.

### Module Manager
- Module state tracking remains unchanged.
- Existing priority-based loading order is preserved.
- Driver module diagnostics use the common module-loading path.

### Notes
- NexusOS 0.5.30 built-in driver modules remain preserved.
- NexusOS 0.5.29 multi-controller xHCI support remains preserved.
- External `.mod` / `.ko` loading remains future work.
- Higher-half kernel work remains intentionally deferred to preserve boot stability.

## NexusOS 0.5.30 — Kernel Driver Modules Foundation

### Added
- Kernel module descriptor ABI for hardware drivers.
- Built-in driver module discovery through the `.nexus_modules` linker section.
- Priority-ordered module loading at kernel startup.
- Runtime module state and load diagnostics.

### Drivers
- PCI, PIC, PIT, input, keyboard, mouse, network, NVMe, AHCI, USB and GPU initialization now runs through the module manager.
- Existing driver APIs and hardware implementations remain intact.

### Notes
- 0.5.29 multi-controller xHCI support is preserved.
- Current modules are built-in kernel modules linked into `kernel.elf`; external `.mod`/`.ko` filesystem loading remains future work.
- Higher-half kernel work remains intentionally deferred to preserve boot stability.

## NexusOS 0.5.29 — Multi-Controller xHCI and Intel USB Compatibility

### Added
- Multi-controller xHCI detection.
- Real-hardware PCI diagnostics for xHCI controllers.
- Active xHCI PCI BDF, vendor/device ID and BAR reporting.
- Controller-specific xHCI initialization diagnostics.

### USB
- All PCI `0C:03:30` xHCI controllers are now enumerated.
- NexusOS no longer stops after the first detected xHCI controller.
- Each detected xHCI controller is attempted until one initializes successfully.
- The generic xHCI driver can operate with standard Intel USB 3.x controllers.
- Existing USB 2/3 root-port and HID keyboard/mouse support remains on the active xHCI backend.

### PCI
- Increased PCI device table capacity for systems with many PCI functions.
- Improved controller discovery on real hardware with multiple USB host controllers.

### Diagnostics
- USB startup now identifies the active xHCI PCI address.
- Vendor/device IDs are printed for the active controller.
- xHCI initialization failures report the failed stage instead of only reporting an unsupported USB controller.

### Notes
- USB4 Host Router / Thunderbolt fabric is detected by the platform but does not use the xHCI driver; dedicated USB4 support remains future work.
- Graphics optimizations from NexusOS 0.5.28 are preserved.
- Userspace shell and process/file-descriptor work remain unchanged.
- Higher-half kernel work remains intentionally deferred to preserve boot stability.

## NexusOS 0.5.28 — USB Controller Compatibility and Graphics Performance

### Added
- USB controller diagnostics for real hardware.
- PCI USB controller count reporting.
- Explicit MMIO mapping for EHCI/OHCI controller registers.
- Fast row-based framebuffer drawing paths.
- Cached wallpaper scaling coordinate maps.
- Cursor save/restore rendering without full desktop redraw on pointer movement.

### USB
- USB initialization now reports whether PCI USB host controllers were detected.
- xHCI remains the preferred controller backend.
- EHCI/OHCI MMIO access is explicitly mapped before controller initialization.
- Initialization failures now expose a useful hardware-side diagnostic instead of only `No supported USB controller`.

### Graphics
- Reworked the software framebuffer renderer to avoid per-pixel function overhead in rectangle drawing.
- Alpha rectangles use packed-channel blending directly on framebuffer words.
- Wallpaper scaling avoids repeated division inside the inner pixel loop.
- Mouse movement no longer forces a full desktop redraw when the desktop state is unchanged.

### Notes
- UEFI GOP remains the active display backend for stable real-hardware output.
- Vendor-specific GPU acceleration and modesetting remain future work.
- The higher-half kernel remains intentionally deferred to preserve boot stability.

## NexusOS 0.5.24 — Per-Process File Descriptor Foundation

### Added
- Fixed-size per-process file descriptor table.
- Standard input, output and error descriptors.
- Descriptor type tracking for future VFS-backed handles.
- `CLOSE` syscall for open descriptors.

### Syscall
- `WRITE` now resolves stdout/stderr through the current process descriptor table.
- Added `NEXUS_SYS_CLOSE`.
- Closed descriptors are rejected by subsequent descriptor-based syscalls.

### Process
- File descriptor state is initialized when a process is created.
- File descriptor state is cleared during safe process reaping.
- Descriptor ownership remains private to each process.

### Notes
- Userspace memory validation from NexusOS 0.5.22 is preserved.
- Userspace console output from NexusOS 0.5.23 is preserved.
- VFS-backed file descriptors and blocking stdin remain future work.

## NexusOS 0.5.23 — Userspace Console Output

### Added
- Bounded userspace `WRITE` syscall.
- stdout/stderr console output from Ring 3.

### Syscall
- Added `NEXUS_SYS_WRITE`.
- Supports file descriptors `1` and `2`.
- Returns the number of bytes written on success.
- Limits one write operation to 4096 bytes.

### Security
- Complete userspace buffers are validated before access.
- User data is copied through the process private CR3.
- Zombie and invalid processes remain blocked by the existing syscall validation.

### Notes
- Syscall validation from NexusOS 0.5.21 is preserved.
- Safe userspace memory access from NexusOS 0.5.22 is now used by a real syscall path.
- Higher-half kernel work remains intentionally deferred to preserve boot stability.

## NexusOS 0.5.22 — Safe Userspace Memory Access

### Added
- Bounded kernel-to-userspace read/write helpers.
- Userspace range validation before cross-address-space copies.
- Writable-page enforcement for kernel writes.

### Process Memory
- Userspace access uses the process private CR3 translation.
- Zombie and invalid processes are rejected by the new access helpers.
- Multi-page reads and writes are handled safely across page boundaries.

### Security
- Kernel writes into userspace require `VMM_PAGE_WRITABLE`.
- Userspace accesses are constrained to the process-owned virtual address range.
- Existing ELF loading remains compatible with executable non-writable mappings.

### Notes
- Syscall validation from NexusOS 0.5.21 is preserved.
- Process lifecycle and safe reaping from NexusOS 0.5.20 are preserved.
- Higher-half kernel work remains intentionally deferred to preserve boot stability.

## NexusOS 0.5.21 — Syscall Validation and Ring-3 ABI Hardening

### Added
- Syscall caller/process ownership validation.
- Ring-3 return-frame validation.
- Userspace RIP mapping validation.
- Userspace writable stack validation.

### Syscalls
- Syscalls are accepted only from scheduler-owned Ring-3 processes.
- Syscall return selectors must match the NexusOS user ABI.
- Ring-3 RFLAGS are validated before returning to userspace.
- Invalid syscall contexts are rejected without modifying scheduler state.

### Process Isolation
- Syscall instruction pointers must belong to mapped executable userspace memory.
- User stack pointers must reference mapped writable userspace memory.
- Zombie and non-user processes cannot enter the syscall dispatcher.

### Notes
- Preserves the private CR3 process model from NexusOS 0.5.18.
- Preserves user page-fault isolation and safe reaping from NexusOS 0.5.19/0.5.20.
- Capture-only hardware IRQ architecture remains unchanged.
- Higher-half kernel work remains intentionally deferred to preserve boot stability.

## 0.5.20 — Enstein — Process lifecycle and safe reaping

- Replaced immediate process resource destruction with a two-phase EXITED/ZOMBIE-style lifecycle using `PROCESS_ZOMBIE`.
- `process_exit()` now records exit metadata and marks the process zombie without destroying the active private CR3.
- Added scheduler-safe `process_reap()` to release user mappings and private page tables only after the process TCB has stopped running.
- Prevented process slots and PIDs from being reused while a process remains a zombie.
- Added process exit-code/reason metadata and zombie-count diagnostics API.
- Hardened scheduler idle handling so a terminated current thread can never be revived as `THREAD_RUNNING` merely because the ready queue is empty.
- Preserved the 0.5.19 user page-fault isolation boundary and capture-only IRQ architecture.

## 0.5.19 — Enstein — User page-fault isolation

- Added recoverable Ring-3 page-fault handling for scheduler-owned user processes.
- Page-fault diagnostics now report CR2, error-code cause, access type, privilege level, RIP and RFLAGS before process termination.
- A user page fault terminates only the faulting process instead of entering the global Kernel Panic path.
- Process teardown is handed to the existing scheduler via `thread_exit()` after `process_exit()`.
- Kernel-mode page faults and all other CPU exceptions remain fatal and use the existing 30-second diagnostic reboot path.
- Preserved capture-only hardware IRQ architecture; no scheduler work was added to IRQ handlers.

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
