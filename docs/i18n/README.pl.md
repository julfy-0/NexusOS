# NexusOS — Polski

NexusOS to lekkki 64-bitowy system operacyjny dla x86_64, napisany od zera w C. Używa własnego bootloadera UEFI i freestanding kernela.

## Funkcje
- Uruchamianie UEFI/GOP i konsola framebuffer
- x86_64 long mode
- GDT, IDT, PIC, PIT i obsługa wyjątków
- Obsługa klawiatury i myszy PS/2
- Podstawa interfejsu graficznego i wiersza poleceń
- Obsługa AHCI/FAT32
- Podstawa obsługi PCI i USB xHCI
- Wybór trybu startu: Graphic lub Command Line
- Ekran Kernel Panic z logami i 30-sekundowym odliczaniem do restartu

## Budowanie
```bash
make clean
make iso
make run
```
