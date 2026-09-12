# STATUS.md — current NexusOS state

## Version

**0.5.3.6 — Enstein**

The current release baseline is the stable identity-mapped NexusOS kernel.
The boot and kernel architecture has now been rewritten internally without
changing the public version number.

## Boot & kernel rewrite

- [x] UEFI GOP framebuffer hand-off
- [x] UEFI boot-volume discovery
- [x] ELF64 validation and PT_LOAD loading
- [x] Relocatable ET_DYN kernel image
- [x] UEFI `R_X86_64_RELATIVE` relocation processing
- [x] Kernel image allocation below 4 GiB
- [x] Boot info and final memory map below 4 GiB
- [x] Runtime kernel physical range passed to PMM
- [x] Private kernel stack established in `_start`
- [x] Early boot contract validation
- [x] GDT / IDT / ISR / PIC / PIT

## Memory

- [x] 4 KiB PMM bitmap allocator
- [x] EFI ConventionalMemory release + reserved-region handling
- [x] Own 4-level page tables
- [x] 2 MiB identity mapping for the low 4 GiB
- [x] 4 KiB VMM mappings
- [x] Kernel heap with `kmalloc` / `kfree`
- [x] Page-fault diagnostics
- [ ] Higher-half kernel
- [ ] Demand paging / swap / COW
- [ ] User/kernel address-space isolation

## Hardware

- [x] PCI enumeration
- [x] AHCI / FAT32 read path
- [x] NVMe read-only driver
- [x] PS/2 keyboard and mouse
- [x] USB UHCI/OHCI/EHCI foundations
- [x] xHCI keyboard path
- [x] GPU detection / metadata
- [x] GOP framebuffer console

## User-facing system

- [x] Shell and command registry
- [x] VFS / mount namespace / FAT32 integration
- [x] GUI desktop, Files, Terminal and Settings
- [x] Desktop Search / Nexus Menu
- [x] `neofetch`, `meminfo`, `sysinfo`, `uname`, `version`
- [x] Timer-driven scheduler foundation
- [x] Threads, TCBs and x86_64 context switching
- [x] Scheduler ready/sleep queues and blocking wakeup
- [x] Synchronization primitives (spinlocks/mutexes)
- [x] Process-safe event wait/notification layer
- [ ] User-space processes
- [ ] Syscall ABI
- [ ] Unified USB HID input
- [ ] Networking

## Validation

The rewritten tree passes:

```text
make clean
make -j2
make iso -j2
make check
```

QEMU runtime validation must be performed on the resulting EFI image in the
developer's environment; this build environment does not provide QEMU.

## Next roadmap block

**0.5.3.6 — Process-safe Event Integration** is implemented. Kernel event
processing now advances per-type sequence counters and wakes blocked kernel
threads through scheduler wait queues. `kernel_events_wait()` provides a
race-safe blocking notification boundary without allowing IRQ handlers to
mutate scheduler lists or switch stacks.

Next block: process/address-space foundation.
