# NexusOS

A standalone 64-bit operating system for x86_64, written primarily in C,
with its own UEFI bootloader (without GRUB). NexusOS uses a monolithic
kernel, custom GDT/IDT/PIC/PIT, PS/2 keyboard and mouse support, framebuffer
graphics, AHCI + FAT32 storage, VFS, a graphical desktop, and a built-in CLI.

> Current version: **0.5.1** — architecture-optimized source tree for future 0.5.2 development.

## 🌐 Languages

- 🇬🇧 **English:** [English](README.md)
- 🇷🇺 **Русский:** [Русский](docs/i18n/README.ru.md)
- 🇺🇦 **Українська:** [Українська](docs/i18n/README.uk.md)
- 🇩🇪 **Deutsch:** [Deutsch](docs/i18n/README.de.md)
- 🇪🇸 **Español:** [Español](docs/i18n/README.es.md)
- 🇫🇷 **Français:** [Français](docs/i18n/README.fr.md)
- 🇵🇱 **Polski:** [Polski](docs/i18n/README.pl.md)
- 🇨🇿 **Čeština:** [Čeština](docs/i18n/README.cs.md)
- 🇨🇳 **中文:** [中文](docs/i18n/README.zh.md)
- 🇯🇵 **日本語:** [日本語](docs/i18n/README.ja.md)

---

## Architecture

NexusOS boots through its own UEFI application, loads the kernel ELF image,
exits UEFI boot services, and transfers control to the x86_64 kernel entry
point. There is no GRUB or BIOS boot path in the current architecture.

The kernel remains intentionally **monolithic**. The source tree is modular
by subsystem, but these modules are linked into one kernel image and execute
in kernel context until future user-mode work changes that model.

### Module boundaries

- `boot/uefi/` — UEFI loader and boot-only assets/headers
- `kernel/core/` — kernel orchestration, state, panic and user-mode foundation
- `kernel/arch/x86_64/` — architecture-specific entry, GDT, IDT, ISR and linker
- `kernel/mm/` — paging and memory-management primitives
- `drivers/` — hardware-facing device drivers
- `fs/` — VFS and concrete filesystems
- `gui/` — GUI state, rendering, desktop, input, search and applications
- `shell/` — shell core, command registry and categorized commands
- `lib/` — freestanding reusable primitives
- `assets/` — replaceable static assets
- `include/nexus/` — shared public NexusOS interfaces
- `platform/` — platform/hardware detection helpers

The module boundaries are organizational and dependency-oriented; they do
not turn NexusOS into a microkernel.

## Project Structure

```text
NexusOS/
├── Makefile
├── build.sh
├── run.sh
├── create-img.sh
├── README.md
├── CHANGELOG.md
├── LICENSE
├── OVMF_VARS.fd
│
├── boot/
│   └── uefi/
│       ├── src/
│       │   └── boot.c
│       ├── include/
│       │   ├── efi.h
│       │   └── elf.h
│       └── assets/
│           ├── nexus_logo.h
│           └── nexus_logo.png
│
├── kernel/
│   ├── core/
│   │   ├── kernel.c
│   │   ├── kstate.c/h
│   │   ├── panic.c/h
│   │   └── usermode/
│   ├── arch/
│   │   └── x86_64/
│   │       ├── entry.S
│   │       ├── gdt.c/h
│   │       ├── gdt_asm.S
│   │       ├── idt.c/h
│   │       ├── io.h
│   │       ├── isr.S
│   │       └── linker.ld
│   ├── mm/
│   │   └── paging.c/h
│   └── bootmode/              # legacy module retained; not linked by default
│
├── drivers/
│   ├── input/
│   │   ├── keyboard/
│   │   └── mouse/
│   ├── graphics/framebuffer/
│   ├── storage/
│   ├── bus/
│   ├── timer/
│   ├── hardware/
│   │   ├── cpu/
│   │   └── pic/
│   └── usb/
│
├── fs/
│   ├── vfs/
│   │   ├── core/
│   │   ├── mount/
│   │   └── registry/
│   └── fat32/
│       ├── fat32.c
│       └── fat32.h
│
├── gui/
│   ├── core/
│   │   ├── gui.c
│   │   ├── gui.h
│   │   ├── gui_state.c
│   │   └── gui_state.h
│   ├── desktop/
│   ├── apps/
│   │   ├── files/
│   │   ├── terminal/
│   │   └── settings/
│   ├── input/
│   ├── renderer/
│   │   ├── renderer.c/h
│   │   └── font.c/h
│   └── search/
│
├── shell/
│   ├── core/
│   │   ├── shell.c/h
│   │   └── command_registry.c/h
│   └── commands/
│       ├── system/
│       ├── filesystem/
│       └── utilities/
│
├── lib/
│   └── memory/
│
├── assets/
│   ├── wallpapers/
│   └── fonts/
│
├── include/
│   └── nexus/
│
├── platform/
│   └── target/
│
├── userdata/
│
└── docs/
    ├── STATUS.md
    ├── ARCHITECTURE.md
    ├── BUILDING.md
    ├── BUILD_SCRIPT.md
    ├── ROADMAP.md
    ├── VERSIONING.md
    ├── AI_HANDOFF.md
    ├── TARGET_HARDWARE.md
    ├── MOUNT_POINTS.md
    ├── architecture/
    ├── development/
    ├── releases/
    ├── adr/
    └── i18n/
```

`build/` and `iso/` are generated directories and are intentionally not part
of the source archive.

## Building

On x86_64 Linux:

```bash
make clean
make
make iso
```

Or use the progress frontend:

```bash
./build.sh
```

The frontend uses GNU Make in parallel. The number of workers can be
overridden:

```bash
NEXUS_BUILD_JOBS=8 ./build.sh
```

For an incremental build:

```bash
./build.sh --no-clean
```

To launch QEMU with UEFI/OVMF:

```bash
./run.sh
# or
make run
```

See [docs/BUILDING.md](docs/BUILDING.md) and
[docs/BUILD_SCRIPT.md](docs/BUILD_SCRIPT.md) for details.

## Current Desktop

The 0.5.1 release includes the graphical desktop, Files, Terminal,
Settings, Nexus Menu, Desktop Search, PS/2 mouse interaction, and keyboard
navigation. The GUI is now split into state/core, renderer, desktop, input,
search and application modules without changing the public `gui_*` API.

## Shell

The existing shell command implementations remain in their functional
categories. `shell/core/command_registry.c` owns the command table and
routing, so adding a command does not require growing a large `if/else`
chain in the shell input loop.

## Development Notes

NexusOS remains freestanding and does not introduce libc or C++ as part of
the kernel. The build system uses the existing host GCC/binutils workflow;
no CMake conversion or new cross-toolchain is required.
