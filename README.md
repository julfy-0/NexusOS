# NexusOS

A standalone 64-bit operating system for x86_64, written from scratch in C,
with its own UEFI bootloader (without GRUB). Monolithic kernel (long mode),
custom GDT/IDT/PIC/PIT, PS/2 keyboard and mouse support, framebuffer console,
AHCI + FAT32 storage support, graphical desktop, and a built-in shell.

> Current version: **0.5.0 - Enstein**

## 🌍 Languages

- 🇬🇧 **Read in English:** [English](README.md)
- 🇷🇺 **Читать на русском:** [Русский](docs/i18n/README.ru.md)
- 🇺🇦 **Читати українською:** [Українська](docs/i18n/README.ua.md)
- 🇩🇪 **Auf Deutsch lesen:** [Deutsch](docs/i18n/README.de.md)
- 🇪🇸 **Leer en español:** [Español](docs/i18n/README.es.md)
- 🇫🇷 **Lire en français:** [Français](docs/i18n/README.fr.md)
- 🇵🇱 **Czytaj po polsku:** [Polski](docs/i18n/README.pl.md)
- 🇨🇿 **Číst v češtině:** [Čeština](docs/i18n/README.cs.md)
- 🇨🇳 **阅读中文版:** [中文](docs/i18n/README.zh.md)
- 🇯🇵 **日本語で読む:** [日本語](docs/i18n/README.ja.md)

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