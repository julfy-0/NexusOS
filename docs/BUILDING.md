# BUILDING.md

## Хорошая новость по сравнению с прежней (BIOS/i386) версией

**Кросс-компилятор не нужен.** Если ты собираешь на x86_64 Linux —
обычный `gcc`/`ld`, которые уже есть в системе, справляются: EFI-
загрузчик — это PE32+ x86-64 (та же архитектура, что у хоста, просто
другой формат исполняемого файла — `ld -m i386pep` умеет его собирать
из стокового `binutils`), ядро — обычный freestanding ELF64 x86-64.

## Зависимости

Для `make` (просто собрать бинарники):
- `gcc`, `binutils` (`ld`) — почти наверняка уже есть

Дополнительно для `make run` (запуск в QEMU):
```bash
sudo apt install qemu-system-x86 ovmf dosfstools mtools
```
- `qemu-system-x86_64` — эмулятор
- `ovmf` — прошивка UEFI для QEMU (`/usr/share/OVMF/OVMF_CODE_4M.fd`
  и `OVMF_VARS_4M.fd`)
- `dosfstools` — даёт `mkfs.vfat` (создать FAT-образ диска)
- `mtools` — даёт `mcopy` (скопировать файлы в FAT-образ без monut)

## Сборка

```bash
make             # build/BOOTX64.EFI + build/kernel.elf
make iso         # + настоящий загрузочный ISO: build/NexusOS-0.5.11.iso
make run         # запускает этот ISO в QEMU с OVMF
make clean       # удалить build/ и iso/
```

`make iso` не требует `xorriso`, `mkisofs`, `mtools` или `dosfstools`.
NexusOS содержит собственный dependency-free генератор `tools/create_iso.py`:
он создаёт ISO9660 и El Torito EFI boot image с FAT16 ESP, внутри которого
находятся `\EFI\BOOT\BOOTX64.EFI` и `\kernel.elf`. Это позволяет напрямую
подключать `build/NexusOS-0.5.11.iso` к VMware как CD/DVD image.

`OVMF_VARS.fd` в корне проекта — рабочая копия NVRAM-переменных
прошивки; `make run` создаёт её сам при первом запуске, если её нет
(копирует эталон из `/usr/share/OVMF/`). Если QEMU не грузится —
попробуй удалить `OVMF_VARS.fd` и запустить `make run` заново (она
могла испортиться после прерванного предыдущего запуска).

## Если что-то не собирается

`make check` — прогонит `-fsyntax-only` по всем `.c` (без загрузчика,
он на другом ABI/формате) — быстрая проверка синтаксиса.

## Отладка

- VMware/QEMU показывает UEFI Shell вместо NexusOS — проверь, что
  `build/NexusOS-0.5.11.iso` создан и что `file build/BOOTX64.EFI` показывает
  `PE32+ ... (EFI application) x86-64`. Структурный El Torito/ISO9660 тест
  выполняется самим `tools/create_iso.py` во время `make iso`.
- Зависание сразу после "Exiting boot services..." — скорее всего,
  что-то не так в `kernel/arch/x86_64/entry.S` или в `gdt_init()`/`idt_init()`
  до того, как консоль успела что-то напечатать; добавь
  `qemu-system-x86_64 ... -no-reboot -d int` для трассировки прерываний
- Клавиатура не отвечает — `drivers/input/keyboard/keyboard.c` явно
  инициализирует контроллер i8042 (не просто читает порт), но если
  всё равно молчит — проверь, что PIC размаскировал IRQ1
  (`kernel/core/kernel.c`: `pic_set_mask(i, i != 0 && i != 1)`)
- `diskls`/`diskcat` говорят "не найден диск" — AHCI не нашёл
  SATA-контроллер или диск не на порту 0; это ожидаемо в минимальной
  QEMU-конфигурации без явно добавленного `-drive` для AHCI (диск
  для FAT-образа NexusOS подключён как обычный `-drive format=raw`,
  это ДРУГОЙ путь чтения — сам NexusOS его не видит через свой
  AHCI-драйвер, только UEFI видел его на этапе загрузчика)

## Creating release media with `build.sh`

`build.sh` now creates both release media formats from the same build artifacts:

```bash
./build.sh
./build.sh
```

Outputs:

- `NexusOS.img` — GPT disk image with BOOT, SYSTEM and USERDATA partitions.

You can also set the total image size directly with `--size`, using `M`, `G`, or `T`, for example `--size 4G` or `--size 1T`. The fixed BOOT and SYSTEM partitions consume 128 MiB; the remaining space becomes USERDATA.
- `NexusOS.iso` — bootable ISO9660/El Torito EFI image suitable for VMware.

The paths can be changed independently:

```bash
./build.sh --size 4G --out NexusOS.img --iso-out NexusOS.iso

For the QEMU launcher, `build.sh` now uses the project-root `NexusOS.img` by default. You can rebuild it at any supported size:

```bash
./build.sh --rebuild --size 4G
./build.sh --rebuild --size 1T
```

The image-size parser accepts `M`, `G`, and `T` suffixes (for example `512M`, `4G`, `1T`). The requested size is the total GPT image size; BOOT and SYSTEM remain fixed at 64 MiB each and the remaining capacity is assigned to USERDATA.
```

## Creating only the ISO with `build.sh`

If you only need a bootable ISO, without creating the GPT disk image, use the
standalone ISO creator:

```bash
./build.sh
./build.sh
```

This creates:

```text
NexusOS.iso
```

Custom output path:

```bash
./build.sh --out build/NexusOS-custom.iso
```

`build.sh` uses the already-built `build/BOOTX64.EFI` and
`build/kernel.elf`; it does not rebuild the kernel or bootloader and does not
create `NexusOS.img`.
