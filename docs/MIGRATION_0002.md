# MIGRATION_0002.md — карта переноса путей (BIOS/i386 → UEFI/x86_64)

Справочник к `docs/adr/0002-uefi-x86_64-pivot.md`. Содержимое файлов
почти не менялось при переносе — только путь. `#include` в перенесённых
файлах остались "плоскими" (`#include "console.h"`, без относительных
путей), поэтому все нужные `-I` пути собраны в Makefile — при
добавлении нового файла в существующую папку ничего менять не нужно.

| Старый путь (проект пользователя) | Новый путь (эта репа)          |
|-------------------------------------|----------------------------------|
| `boot/efi/boot.c`, `efi.h`, `elf.h` | `boot/efi/`                      |
| `boot/kernel/entry.S`               | `arch/x86_64/entry.S`            |
| `boot/kernel/gdt.c/h`, `gdt_asm.S`  | `arch/x86_64/`                   |
| `boot/kernel/idt.c/h`, `isr.S`      | `arch/x86_64/`                   |
| `boot/kernel/kernel.ld`             | `arch/x86_64/linker.ld`          |
| `vendor/io.h`                       | `arch/x86_64/io.h`               |
| `boot/kernel/kernel.c`              | `kernel/kernel.c`                |
| `boot/kernel/kstate.c/h`            | `kernel/kstate.c/h`              |
| `system/shell.c/h`                  | `kernel/shell/shell.c/h`         |
| `system/apps/*.c/h` (50 файлов)     | `kernel/shell/apps/`             |
| `vendor/cpu/*`                      | `drivers/cpu/`                   |
| `vendor/display/*` (+font8x16.h)   | `drivers/console/`               |
| `vendor/input/keyboard.*`           | `drivers/keyboard/`              |
| `vendor/interrupt/pic.*`            | `drivers/pic/`                   |
| `vendor/timer/pit.*`                | `drivers/timer/`                 |
| `ms/storage/pci.*`, `ahci.*`        | `drivers/storage/`               |
| `ms/fs/fat32.*`                     | `fs/`                            |
| `common/mem.c`                      | `lib/mem.c`                      |
| `common/boot_info.h`                | `include/nexus/boot_info.h`      |
| `userdata/`                         | `userdata/` (без изменений)      |
| `extras/c-practice/`                | **не перенесено** — личные C-упражнения автора, не относятся к ОС |
| `build/`, `iso/`                    | не переносится — генерируется Makefile заново |

## Что реально изменилось в содержимом (не просто путь)

- Удалён `kernel/kernel.h` из старой BIOS-версии (описывал Multiboot,
  больше не применим — ничего его не подключало)
- Удалён `include/nexus/types.h` из старой BIOS-версии (алиасы
  `u8/u32/...` не использовались новым кодом — везде `stdint.h` напрямую)
- Makefile переписан с нуля под новую раскладку путей и цели
  (`bootloader`, `kernel`, `iso`, `run`) — логика собрки (флаги,
  порядок объектников) сохранена как в оригинале
