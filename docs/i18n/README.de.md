# NexusOS — Deutsch

NexusOS ist ein leichtgewichtiges 64-Bit-Betriebssystem für x86_64, das vollständig in C entwickelt wird. Es verwendet einen eigenen UEFI-Bootloader und einen freestanding Kernel.

## Funktionen
- UEFI/GOP-Boot und Framebuffer-Konsole
- x86_64 Long Mode
- GDT, IDT, PIC, PIT und Ausnahmebehandlung
- PS/2-Tastatur- und Mausunterstützung
- Grundlage für grafische Oberfläche und Kommandozeile
- AHCI/FAT32-Unterstützung
- Grundlage für PCI- und USB-xHCI-Unterstützung
- Boot-Auswahl: Graphic oder Command Line
- Kernel-Panic-Bildschirm mit Logs und 30-Sekunden-Countdown zum Neustart

## Build
```bash
make clean
make iso
make run
```
