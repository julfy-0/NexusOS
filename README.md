# NexusOS 0.5.2 — System Foundation


## Nexus System

NexusOS 0.5.2 introduces a high-level **Nexus System** around the existing
Nexus Kernel. The kernel remains a custom freestanding x86_64 ELF kernel; the
new system layer owns system state, sessions, power policy, application
registration and the `.nx` package foundation.

The production disk image is a real GPT disk with three FAT32 partitions:

```text
BOOT      64 MiB   -> /boot
SYSTEM    64 MiB   -> /system
USERDATA  selected -> /userdata
```

Build an image with `./create-img.sh --userdata 1G`. The image builder is
dependency-free beyond Python 3 and creates actual GPT/FAT32 structures.

A standalone 64-bit operating system for x86_64, written primarily in C,
with its own UEFI bootloader (without GRUB). NexusOS uses a monolithic
kernel, custom GDT/IDT/PIC/PIT, PS/2 keyboard and mouse support, framebuffer
graphics, AHCI + FAT32 storage, VFS, a graphical desktop, and a built-in CLI.

> Current development version: **0.5.2 — System Foundation**

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

The current kernel is intentionally monolithic. GUI, shell, filesystem,
driver, memory-management, and architecture code are separated by source
directories, while remaining part of the same kernel image.

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
│       ├── src/               # UEFI entry point and loader logic
│       ├── include/           # UEFI-private headers
│       └── assets/            # Bootloader assets
│
├── kernel/
│   ├── core/                  # kmain, kernel state, panic, usermode foundation
│   ├── arch/x86_64/           # GDT, IDT, ISR stubs, entry, linker script
│   ├── mm/                    # Kernel paging / memory-management code
│   └── bootmode/              # Existing boot-mode module (not part of the default link)
│
├── drivers/
│   ├── input/
│   │   ├── keyboard/
│   │   └── mouse/
│   ├── storage/               # AHCI/SATA
│   ├── bus/                   # PCI
│   ├── graphics/framebuffer/  # Framebuffer console
│   ├── timer/                 # PIT
│   ├── hardware/
│   │   ├── cpu/
│   │   └── pic/
│   └── usb/                   # xHCI
│
├── fs/
│   ├── vfs/
│   │   ├── core/              # RAM-backed VFS and GUI-facing VFS API
│   │   ├── mount/             # Mount namespace
│   │   └── registry/          # Filesystem registry
│   └── fat32/                 # FAT32 filesystem implementation
│
├── gui/
│   ├── core/                  # Desktop state and GUI orchestration
│   └── renderer/              # Font abstraction / rendering support
│
├── shell/
│   ├── core/                  # Shell parser/input/command dispatch
│   └── commands/
│       ├── system/            # System and hardware commands
│       ├── filesystem/        # Filesystem commands
│       └── utilities/         # General utilities and math/text commands
│
├── lib/
│   └── memory/                # Freestanding memory primitives
│
├── assets/
│   ├── wallpapers/
│   └── fonts/
│
├── include/
│   └── nexus/                 # Shared/public NexusOS headers
│
├── platform/
│   └── target/                # Hardware/platform detection
│
├── userdata/                  # Runtime/user-data layout
│
└── docs/
    ├── architecture/
    ├── development/
    ├── releases/
    ├── adr/
    ├── i18n/
    ├── STATUS.md
    ├── ARCHITECTURE.md
    ├── BUILDING.md
    ├── BUILD_SCRIPT.md
    ├── ROADMAP.md
    └── VERSIONING.md
```

`build/` and `iso/` are generated directories and are intentionally not part
of the source tree.

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

The 0.5.2 development target includes the graphical desktop, Files, Terminal,
Settings, Nexus Menu, Desktop Search, PS/2 mouse interaction, and keyboard
navigation. The existing CLI remains available through `desktop-run`.

## Development Notes

NexusOS remains freestanding and does not introduce libc or C++ as part of
the kernel. The build system uses the existing host GCC/binutils workflow;
no CMake conversion or new cross-toolchain is required.
