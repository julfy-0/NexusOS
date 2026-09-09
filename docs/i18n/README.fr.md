# NexusOS — Français

NexusOS est un système d’exploitation léger 64 bits pour x86_64, écrit entièrement en C. Il utilise un chargeur UEFI personnalisé et un noyau freestanding.

## Fonctionnalités
- Démarrage UEFI/GOP et console framebuffer
- x86_64 en long mode
- GDT, IDT, PIC, PIT et gestion des exceptions
- Prise en charge du clavier et de la souris PS/2
- Base d’interface graphique et ligne de commande
- Prise en charge AHCI/FAT32
- Base de prise en charge PCI et USB xHCI
- Choix du mode de démarrage : Graphic ou Command Line
- Écran de Kernel Panic avec journaux et compte à rebours de 30 secondes

## Compilation
```bash
make clean
make iso
make run
```
