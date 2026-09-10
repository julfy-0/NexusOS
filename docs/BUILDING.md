# BUILDING.md

## Toolchain

На x86_64 Linux отдельный cross-compiler не требуется. Текущий build использует
обычный `gcc`/`ld`:

- UEFI loader — PE32+ x86-64 через `ld -m i386pep`
- Kernel — freestanding ELF64 x86-64

## Dependencies

Для обычной сборки:

- `gcc`
- `binutils`
- `make`

Для QEMU/image workflow:

```bash
sudo apt install qemu-system-x86 ovmf dosfstools mtools
```

Нужны `qemu-system-x86_64`, OVMF, `mkfs.vfat`, `mcopy` и `mmd`.

## Build commands

```bash
make clean
make
make iso
```

Или:

```bash
./build.sh
```

Количество параллельных jobs:

```bash
NEXUS_BUILD_JOBS=8 ./build.sh
```

Инкрементальная сборка:

```bash
./build.sh --no-clean
```

Проверка C-синтаксиса:

```bash
make check
```

Запуск:

```bash
make run
# или
./run.sh
```

## What each target does

- `make` — собирает `build/BOOTX64.EFI` и `build/kernel.elf`
- `make iso` — добавляет UEFI ESP layout в `iso/`
- `make run` — запускает существующий QEMU frontend
- `make clean` — удаляет generated `build/` и `iso/`
- `make check` — делает `-fsyntax-only` по kernel-side C и UEFI C

## Debugging

Если UEFI Shell появляется вместо NexusOS, проверь:

```text
iso/EFI/BOOT/BOOTX64.EFI
```

и:

```bash
file build/BOOTX64.EFI
file build/kernel.elf
```

Если зависание происходит сразу после выхода из boot services, смотри
`kernel/arch/x86_64/entry.S`, GDT, IDT и раннюю инициализацию paging.

Если keyboard не отвечает, смотри `drivers/input/keyboard/keyboard.c` и
PIC IRQ1. GUI keyboard handling теперь находится выше драйвера в
`gui/input/input.c`.

Если `diskls`/`diskcat` не видят диск, это отдельный AHCI path; FAT image,
который QEMU использует для загрузки, не автоматически означает, что NexusOS
видит тот же storage controller через AHCI.
