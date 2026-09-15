# NexusOS

NexusOS is a freestanding x86_64 operating-system project built around a UEFI boot path, a monolithic kernel, hardware drivers, a command-line shell and a framebuffer desktop.

## Current release

**NexusOS 0.5.24 — Enstein**

The current milestone is the **Writable FAT32 Foundation**. NexusOS now has a bounded real-disk write path beneath the existing App Manager/Application Discovery stack.

## Build

From WSL/Linux with the required cross/freestanding toolchain installed:

```bash
make clean
make -j2 iso
make check
```

The bootable ISO is produced at `build/NexusOS-0.5.24.iso`.

For a standalone ISO from an already-built tree:

```bash
./create-iso.sh
```

## Project documentation

See `docs/` for architecture, building, networking, shell I/O, filesystem, release and roadmap documentation.
