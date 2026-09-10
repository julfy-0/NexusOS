# NexusOS AI Handoff

## Current baseline

NexusOS is an existing x86_64 freestanding C operating system with a custom
UEFI bootloader and monolithic kernel. Do not replace it with a demo or rewrite
working subsystems.

## Current architecture

```text
boot/uefi        UEFI loader
kernel/          kernel and x86_64 architecture
 drivers/        hardware
fs/              GPT, VFS and FAT32
system/          high-level Nexus System services
gui/             existing desktop implementation
shell/           existing shell and commands
assets/          source assets
include/nexus/   shared contracts
```

## Important invariants

- `kernel.elf` remains the kernel artifact.
- UEFI remains the boot mechanism.
- No GRUB.
- Kernel remains freestanding C.
- Existing CLI, GUI, PS/2 input, VFS, FAT32 and AHCI functionality must be
  preserved.
- `.nx` is metadata/package infrastructure only until an executable model is
  designed.
- BOOT/SYSTEM/USERDATA is a real GPT layout, not three folders in one image.

## Image layout

```text
BOOT      64 MiB FAT32  -> /boot
SYSTEM    64 MiB FAT32  -> /system
USERDATA  configurable -> /userdata
```

The UEFI loader finds SYSTEM through its GPT partition type and loads
`SYSTEM/KERNEL/KERNEL.ELF`. A fallback to the old BOOT-root `kernel.elf` exists
for migration safety.

## Build

```bash
make clean
make
make iso
./build.sh
./create-img.sh --userdata 1G
./run.sh
```
