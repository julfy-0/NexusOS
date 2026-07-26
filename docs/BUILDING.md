# BUILDING.md

## Зависимости

Нужны на машине, где реально собираешь (не в песочнице разработки —
там нет сети):

- `i686-elf-gcc` + `i686-elf-binutils` — кросс-компилятор.
  Собери через `./toolchain/build-cross-compiler.sh` (см. требования
  внутри скрипта — нужны `build-essential bison flex libgmp-dev
  libmpc-dev libmpfr-dev texinfo`), либо поставь готовый пакет, если
  дистрибутив его даёт (например `pacman -S i686-elf-gcc` на Arch
  через AUR).
- `grub-mkrescue` (пакет `grub-pc-bin` + `xorriso` на Debian/Ubuntu)
  — для сборки загрузочного `.iso`
- `qemu-system-i386` — для запуска без реального железа

Проверка:
```bash
i686-elf-gcc --version
grub-mkrescue --version
qemu-system-i386 --version
```

## Сборка

```bash
export PATH="$HOME/opt/cross/bin:$PATH"   # если собирал toolchain сам

make            # собрать build/nexuskernel.bin
make iso        # собрать build/NexusOS.iso
make run        # собрать iso и запустить в QEMU
make run-kernel # запустить сырое ядро в QEMU напрямую (без ISO, быстрее)
make clean      # удалить всё собранное
```

## Если нет кросс-компилятора под рукой

`make check` — прогонит все `.c`-файлы через обычный host `gcc` с
`-fsyntax-only -m32` — проверит синтаксис и типы, но **не соберёт
рабочий образ** (нет верной линковки под bare-metal i386-target).
Это только для быстрой проверки "не сломал ли я C-код", не замена
настоящей сборки.

## Отладка

- `-serial stdio` (уже включено в `make run`) выводит COM1-лог в
  терминал — полезно, если паника случилась раньше инициализации VGA
  или экран QEMU не открылся
- Для отладки на уровне инструкций: `qemu-system-i386 -cdrom
  build/NexusOS.iso -s -S` открывает GDB-stub на порту 1234, дальше
  `gdb build/nexuskernel.bin` → `target remote localhost:1234`
- `objdump -d build/nexuskernel.bin` — дизассемблировать собранное
  ядро, если непонятно, что происходит на уровне машинного кода

## Частые проблемы

- **"undefined reference to memcpy/memset" при линковке своего кода** —
  значит где-то забыл `#include <lib/string.h>` или компилятор сам
  сгенерировал вызов libc-функции (`-fno-builtin` в Makefile должен
  это предотвращать, но если добавляешь новый файл вручную — не
  забудь такие же флаги)
- **PIC/IRQ не приходят** — проверь, что `sti` вызван ПОСЛЕ
  `irq_init()` и регистрации всех обработчиков (`kernel_main`
  соблюдает этот порядок специально)
- **Тройной фолт сразу после GDT** — почти всегда неправильный
  селектор в `gdt_flush` (см. `arch/i386/gdt.c`) или сломанный `.bss`
  для стека (`boot/boot.S`)
