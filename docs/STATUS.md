# NexusOS 0.5.37 — Enstein

## Kernel Driver Modules Foundation

Driver initialization now goes through the kernel module manager. Hardware drivers are registered as built-in modules with metadata, priorities, runtime state and common load handling. Dynamic external module files remain future work.

## App Manager Foundation

NexusOS 0.5.13 retains the App Manager and Application Discovery foundations
and adds the first writable storage boundary needed for future installation.

- Built-in applications are registered through a dedicated helper.
- Applications have stable IDs, names, entry metadata and an explicit source:
  `builtin`, `system` or `userdata`.
- The manager tracks `stopped` / `running` state and launch counts.
- Only one application is considered active at a time.
- Stopping the active application returns the system to the desktop state.
- The lifecycle API does not pretend to execute binaries: ELF loading and
  userspace process execution remain future work.

The existing storage mount points `/system/apps` and `/userdata/apps` remain the
reserved locations for packaged applications. Discovery is still read-only at the
package-manager level, while the underlying FAT32/VFS backend can now create
directories and create/overwrite bounded 8.3 files.

## Application Discovery

NexusOS 0.5.13 retains the read-only application discovery path introduced in
0.5.11. The package manager scans `/system/apps` and
`/userdata/apps`, parses `manifest.nxm`, and registers valid applications with
the matching source classification.

Discovery itself does not install, extract, or execute applications. ELF loading
and userspace execution remain separate milestones. The new writable FAT32
primitive is intentionally lower-level and is not yet exposed as a package
installer.

## Networking

The 0.5.9 E1000 networking foundation remains integrated. ARP, IPv4, ICMP, UDP
and TCP are still future work.

## Writable FAT32 Foundation

- AHCI supports bounded `WRITE DMA EXT` sector commands.
- FAT32 can allocate/free clusters and update mirrored FAT copies.
- FAT32 can create directories and create/overwrite short 8.3 files.
- The active `/mnt/disk0` mount is read-write.
- VFS `mkdir`, `touch`, and `write` route to FAT32 when the resolved mount is writable.
- LFN, journaling, permissions, unlink/rename, transactions and executable loading remain future work.


## 0.5.13 — Package Installation Transaction

The Package Manager now provides a conservative transactional installation path. A package directory is validated before mutation, an application directory is staged, the optional single Entry payload is copied with a bounded 64 KiB limit, and `manifest.nxm` is written last as the publication point. Failures roll back the newly created payload and directory when possible.


## 0.5.14 — Multi-file package payload

The Package Manager now accepts an optional `Files` manifest field for up to eight root-level 8.3 payload files. Every payload is preflight-validated before mutation, copied into the staging directory, and removed in reverse order on rollback. `manifest.nxm` remains the final publication point.


## 0.5.15 — ELF64 user-space loader foundation

NexusOS can now validate and load small ELF64 x86_64 executables from the
mounted FAT32 filesystem. `elf-run <path>` creates a process, maps the ELF
load segments with user permissions, allocates a user stack and enters Ring 3
through the existing IRETQ transition.

This is intentionally a synchronous execution foundation: the current active
CR3 is still shared, syscall entry/return is not enabled, and scheduler-owned
user processes remain future work.


## Syscall entry/return

NexusOS 0.5.17 adds the first user-visible syscall entry path through `INT 0x80`. The IDT gate is DPL3, the entry stub saves all general-purpose registers, dispatches NOP/GETPID, returns the result in RAX, and restores the original CPL3 IRETQ frame. `EXIT` remains deferred until scheduler-owned user processes exist; the entry path deliberately refuses to return into an already-terminated process.


## Private CR3 / Process Address Spaces

NexusOS 0.5.18 gives each process a private copy of the kernel page-table hierarchy. User mappings are created only in that process CR3, and the scheduler switches CR3 together with the scheduler TCB. User image initialization no longer relies on the active kernel virtual address for the destination; it writes through the process page-table translation into the physical user pages.

The kernel identity map remains available in each address space so existing kernel code and hardware mappings continue to work. The higher-half kernel remains intentionally deferred.


## User Page-Fault Isolation

NexusOS 0.5.20 treats a page fault taken while executing a scheduler-owned Ring-3 process as a process-local failure. The handler prints the fault address and error-code details, terminates the current process, and transfers ownership back to the scheduler. Kernel-mode page faults remain fatal.


## 0.5.21 — Syscall Validation and Ring-3 ABI Hardening

NexusOS 0.5.21 hardens the tested `INT 0x80` user ABI. Syscalls now require a
valid scheduler-owned Ring-3 process, and the saved return frame is validated
before NexusOS executes `IRETQ`. The user instruction pointer must reference a
mapped executable page and the user stack pointer must reference mapped writable
memory. Invalid contexts are rejected without changing scheduler ownership.


## 0.5.22 — Safe userspace memory access

NexusOS 0.5.22 adds explicit bounded userspace memory-access helpers for future syscall arguments and kernel services. Kernel writes now have a dedicated path that requires writable user mappings, while kernel reads validate the complete userspace range before translating each page through the process private CR3. Existing ELF initialization keeps its lower-level loader path so executable read-only pages can still be populated safely before userspace starts.


## 0.5.23 — Userspace console write syscall

NexusOS 0.5.23 extends the validated `INT 0x80` ABI with a bounded,
non-blocking userspace console write operation. The syscall accepts stdout or
stderr, validates the complete userspace buffer through the process private
CR3, copies it into a bounded kernel buffer, and emits the bytes through the
existing console path.




## 0.5.28 — USB controller compatibility and graphics performance

NexusOS 0.5.28 hardens real-hardware USB bring-up with PCI controller diagnostics and explicit MMIO mapping for legacy EHCI/OHCI paths. The GUI framebuffer renderer now uses row-based drawing, cached wallpaper coordinate maps and cursor restoration so mouse movement does not trigger a full-screen redraw. UEFI GOP remains the active display backend; vendor-specific GPU acceleration remains future work.
## 0.5.24 — Per-process file descriptor foundation

NexusOS 0.5.24 adds a fixed per-process descriptor table as the kernel-side
foundation for userspace file I/O. Every process receives stdin/stdout/stderr
entries, the existing `WRITE` syscall resolves output through that table, and
`CLOSE` can disable an open descriptor. Descriptor state is reset during safe
process reaping; VFS-backed file objects and blocking stdin remain future work.


## 0.5.29 — Multi-controller xHCI and Intel USB compatibility

The USB subsystem now enumerates every PCI xHCI controller and attempts each controller in turn. The active backend exposes PCI BDF and vendor/device diagnostics for real-hardware bring-up. Standard Intel USB 3.x controllers use the same generic xHCI driver; USB4 Host Router support remains separate future work.
