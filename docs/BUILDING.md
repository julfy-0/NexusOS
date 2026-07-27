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
make iso         # + iso/EFI/BOOT/BOOTX64.EFI, iso/kernel.elf (структура ESP)
make run         # + build/fat.img, запуск в QEMU с OVMF
make clean       # удалить build/ и iso/
```

`OVMF_VARS.fd` в корне проекта — рабочая копия NVRAM-переменных
прошивки; `make run` создаёт её сам при первом запуске, если её нет
(копирует эталон из `/usr/share/OVMF/`). Если QEMU не грузится —
попробуй удалить `OVMF_VARS.fd` и запустить `make run` заново (она
могла испортиться после прерванного предыдущего запуска).

## Если что-то не собирается

`make check` — прогонит `-fsyntax-only` по всем `.c` (без загрузчика,
он на другом ABI/формате) — быстрая проверка синтаксиса.

## Отладка

- QEMU-окно с UEFI Shell вместо загрузки NexusOS — значит,
  `BOOTX64.EFI` не нашёлся или не подходит по формату/архитектуре;
  проверь, что `iso/EFI/BOOT/BOOTX64.EFI` реально существует и что
  `file build/BOOTX64.EFI` показывает `PE32+ ... (EFI application)
  x86-64`
- Зависание сразу после "Exiting boot services..." — скорее всего,
  что-то не так в `arch/x86_64/entry.S` или в `gdt_init()`/`idt_init()`
  до того, как консоль успела что-то напечатать; добавь
  `qemu-system-x86_64 ... -no-reboot -d int` для трассировки прерываний
- Клавиатура не отвечает — `drivers/keyboard/keyboard.c` явно
  инициализирует контроллер i8042 (не просто читает порт), но если
  всё равно молчит — проверь, что PIC размаскировал IRQ1
  (`kernel/kernel.c`: `pic_set_mask(i, i != 0 && i != 1)`)
- `diskls`/`diskcat` говорят "не найден диск" — AHCI не нашёл
  SATA-контроллер или диск не на порту 0; это ожидаемо в минимальной
  QEMU-конфигурации без явно добавленного `-drive` для AHCI (диск
  для FAT-образа NexusOS подключён как обычный `-drive format=raw`,
  это ДРУГОЙ путь чтения — сам NexusOS его не видит через свой
  AHCI-драйвер, только UEFI видел его на этапе загрузчика)
