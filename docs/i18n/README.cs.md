# NexusOS — Čeština

NexusOS je lehký 64bitový operační systém pro x86_64, napsaný od nuly v jazyce C. Používá vlastní UEFI bootloader a freestanding kernel.

## Funkce
- Spouštění přes UEFI/GOP a framebufferová konzole
- x86_64 long mode
- GDT, IDT, PIC, PIT a zpracování výjimek
- Podpora klávesnice a myši PS/2
- Základ grafického rozhraní a příkazového řádku
- Podpora AHCI/FAT32
- Základ podpory PCI a USB xHCI
- Volba režimu spuštění: Graphic nebo Command Line
- Kernel Panic obrazovka s logy a 30sekundovým odpočtem do restartu

## Sestavení
```bash
make clean
make iso
make run
```
