# NexusOS — Español

NexusOS es un sistema operativo ligero de 64 bits para x86_64, escrito desde cero en C. Utiliza un cargador UEFI propio y un kernel freestanding.

## Funciones
- Arranque UEFI/GOP y consola framebuffer
- x86_64 en long mode
- GDT, IDT, PIC, PIT y manejo de excepciones
- Soporte para teclado y ratón PS/2
- Base de interfaz gráfica y línea de comandos
- Soporte AHCI/FAT32
- Base de soporte PCI y USB xHCI
- Selección de arranque: Graphic o Command Line
- Pantalla de Kernel Panic con registros y cuenta atrás de 30 segundos

## Compilación
```bash
make clean
make iso
make run
```
