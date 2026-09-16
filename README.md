# NexusOS

NexusOS is a freestanding x86_64 operating-system project built around a UEFI boot path, a monolithic kernel, hardware drivers, a command-line shell and a framebuffer desktop.

## Current release

**NexusOS 0.6.0 — Enstein**

The current milestone is the **NexusOS 0.6.0 Platform Integration**. This release expands the userspace ABI, process metadata, VFS-backed descriptors, memory mapping foundation, scheduler accounting and the embedded Roboto font system.

## Build

From WSL/Linux with the required cross/freestanding toolchain installed:

```bash
make clean
make -j2 iso
make check
```

The bootable ISO is produced at `build/NexusOS-0.6.0.iso`.

For a standalone ISO from an already-built tree:

```bash
./build.sh
```

## Project documentation

See `docs/` for architecture, building, networking, shell I/O, filesystem, release, roadmap and the current 0.6.0 implementation set.


### NexusOS 0.6.0

The 0.6.0 integration milestone is tracked in `docs/0.6.0_PLAN.md` with a 128+ feature target and 200 hardening/fix target.

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
