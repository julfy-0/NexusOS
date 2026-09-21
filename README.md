# NexusOS

NexusOS is a freestanding x86_64 operating-system project built around a UEFI boot path, a monolithic kernel, hardware drivers, a command-line shell and a framebuffer desktop.

## Current release

**NexusOS 0.7 — Enstein**

The current milestone is **NexusOS 0.7 — Native Userspace Runtime**. This release begins the Nexus Native Architecture with a public syscall ABI, Nexus Runtime service, and cross-process Nexus Channel IPC while preserving the 0.6.x stability work.

## Build

From WSL/Linux with the required cross/freestanding toolchain installed:

```bash
make clean
make -j2 iso
make check
```

The bootable ISO is produced at `build/NexusOS-0.7.iso`.

For a standalone ISO from an already-built tree:

```bash
./build.sh
```

## Project documentation

See `docs/` for architecture, building, networking, shell I/O, filesystem, release, roadmap and the 0.7 native runtime foundation.

The Native Runtime userspace library also provides `runtime_context.h`/`runtime_context.c` for reusable runtime state snapshots. Build it with `make user-runtime`.


### NexusOS 0.7

The 0.7 line begins the Nexus Native Architecture. The 0.6.x line is treated as the stability foundation.

System font assets: `assets/fonts/Roboto-Regular.ttf` and `assets/fonts/Roboto-Bold.ttf`.

## Build and image creation

The main build command now also creates the disk image and standalone ISO:

```bash
./build.sh
```

Custom total image sizes are supported:

```bash
./build.sh --size 1G
./build.sh --size 16G
./build.sh --size 1T
```

To rebuild only the image/media from existing build artifacts:

```bash
./build.sh --image-only --size 4G
```

`NexusOS.img` and `NexusOS.iso` are created in the project root by default.


### 0.6.0 Display modes and framebuffer
NexusOS enumerates UEFI GOP modes and prefers the largest available pixel-area mode. The GUI can render into a kernel backbuffer and present it to the firmware framebuffer. UEFI GOP basic mode information does not expose refresh rate; exact arbitrary-Hz switching remains GPU-driver work.
