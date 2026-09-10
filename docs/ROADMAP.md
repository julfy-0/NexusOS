# ROADMAP.md — путь развития NexusOS

Этот roadmap отражает фактическое состояние кода. Версии не меняются только
из-за реорганизации дерева: текущий release остаётся **0.5.1**.

## Milestone 0.3 — refit — закрыт

- [x] UEFI/x86_64 pivot
- [x] Собственная UEFI-загрузка и long-mode kernel
- [x] Реальная сборка и живой QEMU+OVMF boot

## Milestone 0.4 — memoria — частично закрыт

- [x] Собственные page tables
- [x] Page fault handler
- [ ] `kmalloc`/`kfree`
- [ ] Higher-half kernel

## Milestone 0.5 — Enstein — закрыт функционально

- [x] VFS path traversal
- [x] `/mnt` и `/mnt/disk0` namespace
- [x] FAT32 mount/read path
- [x] Read-only FAT32 mount protection

## 0.5.1 — Desktop Update — текущий release

- [x] Graphical desktop
- [x] Files / Terminal / Settings
- [x] Nexus Menu / Desktop Search
- [x] Keyboard + mouse GUI interaction
- [x] Parallel build frontend

## 0.5.2 preparation — architecture stabilization

This is a development target, not a release-version change in the current
archive.

- [x] GUI lifecycle/state separated from rendering
- [x] GUI desktop, search and applications separated into modules
- [x] GUI input separated from rendering
- [x] FAT32 implementation grouped under `fs/fat32/`
- [x] Shell command dispatch moved to a command registry
- [x] Documentation synchronized with the actual source tree
- [x] Build system continues to discover modules automatically

## Next functional milestone — threadwork

- [ ] `task_t` / TCB
- [ ] x86_64 context switching
- [ ] Scheduler
- [ ] Move keyboard input from direct IRQ→shell execution to an event queue
- [ ] Make long-running work independent of IRQ context

## User mode

- [ ] TSS and ring 3 transition
- [ ] syscall/sysret interface
- [ ] Process object and address-space ownership
- [ ] Move shell and commands into a user-space process without rewriting
      their user-visible command behavior
- [ ] Minimal user-space libc

## Storage / filesystem

- [x] PCI enumeration
- [x] AHCI/SATA detection
- [x] FAT32 mounting and reading
- [ ] FAT32 write support, if/when required
- [ ] Additional filesystem backend(s)
- [ ] Expand VFS backend abstraction beyond the current scope

## GUI / desktop

- [x] Modular GUI state/core
- [x] Renderer boundary and bitmap-font abstraction
- [x] Desktop / dock / search modules
- [x] Files / Terminal / Settings application modules
- [ ] Window object and focus model
- [ ] Window manager
- [ ] Optional compositor, only when actually required
- [ ] Bitmap conversion pipeline for future Inter font assets

## Later

- Networking
- SMP
- Journaling filesystem
- Power-management improvements

## 1.0 criteria

Not just a checklist: NexusOS should have a self-contained boot path,
multiprocessing or multitasking, stable filesystem facilities, isolated
user-space processes, and an interactive shell running outside kernel context.
