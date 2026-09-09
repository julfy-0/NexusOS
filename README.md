# NexusOS

A standalone 64-bit operating system for x86_64, written from scratch in C,
with its own UEFI bootloader (without GRUB). Monolithic kernel (long mode),
custom GDT/IDT/PIC/PIT, PS/2 keyboard and mouse support, framebuffer console,
AHCI + FAT32 storage support, graphical desktop, and a built-in shell.

> Current version: **0.5.0 - Enstein**

## 🌍 Languages

- 🇬🇧 **Read in English:** [English](README.md)
- 🇷🇺 **Читать на русском:** [Русский](README_RU.md)
- 🇺🇦 **Читати українською:** [Українська](README_UA.md)
- 🇩🇪 **Auf Deutsch lesen:** [Deutsch](README_DE.md)
- 🇪🇸 **Leer en español:** [Español](README_ES.md)
- 🇫🇷 **Lire en français:** [Français](README_FR.md)
- 🇵🇱 **Czytaj po polsku:** [Polski](README_PL.md)
- 🇨🇿 **Číst v češtině:** [Čeština](README_CS.md)
- 🇨🇳 **阅读中文版：** [中文](README_ZH.md)
- 🇯🇵 **日本語で読む：** [日本語](README_JA.md)

---

## Project Structure

```text
NexusOS/

├── Makefile
├── build.sh
├── run.sh
├── create-img.sh
├── OVMF_VARS.fd
│
├── boot/
│   └── efi/                    # UEFI bootloader
│
├── arch/
│   └── x86_64/                 # Architecture-specific code
│
├── kernel/
│   ├── gui/                    # Graphical desktop
│   └── shell/                  # NexusOS Command Line
│
├── drivers/
│   ├── console/
│   ├── cpu/
│   ├── keyboard/
│   ├── mouse/
│   ├── pic/
│   ├── timer/
│   └── storage/
│
├── fs/                         # Filesystem support
├── lib/                        # Freestanding library
├── include/                    # Public headers
├── userdata/                   # User data
│
└── docs/
    ├── STATUS.md
    ├── ROADMAP.md
    ├── ARCHITECTURE.md
    ├── BUILDING.md
    └── ...