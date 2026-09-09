# NexusOS — English

NexusOS is a lightweight 64-bit x86_64 operating system written from scratch in C. It uses a custom UEFI bootloader and a freestanding kernel.

## Features
- UEFI/GOP boot and framebuffer console
- x86_64 long mode
- GDT, IDT, PIC, PIT and exception handling
- PS/2 keyboard and mouse support
- Graphical UI foundation and command-line shell
- AHCI/FAT32 storage support
- PCI and xHCI USB foundation
- Boot mode selection: Graphic or Command Line
- Kernel panic screen with logs and a 30-second restart countdown

## Build
```bash
make clean
make iso
make run
```

See the other files in this directory for translations.
