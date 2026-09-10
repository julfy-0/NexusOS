# NexusOS 0.5.2 — Status

## Release target

**0.5.2 — System Foundation**

This development target preserves the 0.5.1 kernel/GUI implementation while
introducing the Nexus System layer and real multi-partition disk image flow.

## Implemented

- [x] Existing custom UEFI bootloader preserved
- [x] Existing x86_64 monolithic kernel preserved
- [x] Existing `kernel.elf` format and entry point preserved
- [x] GPT partition discovery in the kernel
- [x] Nexus System Core
- [x] Explicit system states: booting/CLI/desktop/application/shutdown/reboot
- [x] `desktop-run` routed through System Core state transition
- [x] Real GPT image builder
- [x] BOOT 64 MiB FAT32 partition
- [x] SYSTEM 64 MiB FAT32 partition
- [x] Configurable USERDATA FAT32 partition
- [x] Dependency-free FAT32 image generation helper
- [x] SYSTEM partition contains `KERNEL/KERNEL.ELF`
- [x] UEFI bootloader searches the SYSTEM partition for the kernel
- [x] Compatibility fallback for old boot images
- [x] VFS `/boot`, `/system`, `/userdata` mount namespace
- [x] Multiple independent FAT32 read-only mount contexts
- [x] Session, power, system-info service boundaries
- [x] Package Manager foundation for `.nx`
- [x] Application Manager foundation
- [x] Built-in Files/Terminal/Settings registrations preserved
- [x] Existing GUI, CLI, keyboard, mouse, VFS, AHCI and paging code retained

## Package system scope

`.nx` currently means **Nexus Package**. The implementation provides metadata
and registration foundations only. It intentionally does not execute package
binaries, download packages, or parse complex archives.

## Not implemented yet

- [ ] Kernel heap (`kmalloc`/`kfree`)
- [ ] Higher-half kernel
- [ ] Processes and preemptive multitasking
- [ ] Ring-3 user mode and system calls
- [ ] Writable FAT32
- [ ] NVMe driver
- [ ] Dynamic `.nx` executable loading
- [ ] Network package repositories / App Store
- [ ] Full LFN FAT32 support
