# NexusOS Architecture

NexusOS 0.5.2 keeps the existing custom monolithic kernel and UEFI bootloader,
but adds a distinct high-level **Nexus System** layer.

## Boot and runtime flow

```text
UEFI
  ↓
BOOT partition / EFI/BOOT/BOOTX64.EFI
  ↓
Nexus Bootloader
  ↓
GPT discovery of SYSTEM partition
  ↓
SYSTEM/KERNEL/KERNEL.ELF
  ↓
Nexus Kernel
  ├── GDT / IDT / PIC / PIT
  ├── paging / interrupts
  ├── framebuffer / console
  └── hardware drivers
  ↓
VFS + FAT32 + GPT storage discovery
  ↓
Nexus System
  ├── system state
  ├── session service
  ├── power service
  ├── system information
  ├── package manager foundation
  └── application manager
  ↓
CLI / Desktop
  ↓
Built-in applications / future .nx packages
```

## Kernel vs Nexus System

The kernel remains responsible for low-level execution and hardware access.
High-level policy belongs under `system/`.

The kernel is still a freestanding ELF64 `kernel.elf`; its binary format and
entry point are unchanged.

## System states

`system/core/state.*` defines:

- `BOOTING`
- `CLI`
- `DESKTOP`
- `APPLICATION`
- `SHUTDOWN`
- `REBOOT`

`desktop-run` now changes the state to `DESKTOP` before starting the existing
GUI. Leaving the desktop returns the state to `CLI`.

## Storage architecture

The disk image is a real GPT disk:

```text
NexusOS.img
├── BOOT       64 MiB   FAT32   /boot
├── SYSTEM     64 MiB   FAT32   /system
└── USERDATA   selected FAT32  /userdata
```

The bootloader uses UEFI Block I/O + Simple File System protocols to locate the
private NexusOS SYSTEM partition type and loads `\kernel\kernel.elf` from it.
For old pre-0.5.2 images it retains a compatibility fallback to
`\kernel.elf` on the boot volume.

After `ExitBootServices`, the kernel discovers the GPT through the existing
AHCI block driver and mounts the FAT32 partitions through the existing VFS.
The FAT32 driver now supports multiple independent read-only contexts instead
of a single global filesystem instance.

## Package architecture

`.nx` is a package container/representation, not a new executable format.
The current package layer only provides:

- package path recognition
- manifest parsing
- package discovery boundary
- application registration boundary

No network downloader, App Store, arbitrary binary execution, or archive parser
has been added.
