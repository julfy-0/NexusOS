# CHANGELOG.md

Формат: `## [версия] — codename` + список изменений. Новое — сверху.

## [0.4.5] — memoria

- `docs/STATUS.md`/`README.md` синхронизированы с реальным состоянием
  кода — до этой записи `STATUS.md` всё ещё числил page tables и page
  fault handler как "не сделано" (раздел "Что НЕ сделано"), хотя они
  уже были в коде (см. 0.4.1/0.4.2) — версия здесь и версия в коде
  разошлись, это тоже баг, просто в документации

## [0.4.4] — memoria

- Единый источник версии в коде — `include/nexus/nexus_version.h`
  (`NEXUS_VERSION_STRING`). Раньше `version`, `neofetch` и `uname`
  хардкодили три РАЗНЫЕ, не совпадающие друг с другом строки
  (`"shell 0.2, kernel 0.3-experimental"` / `"0.3-experimental"` дважды),
  и ни одна не совпадала с версией из `STATUS.md`. Теперь все три
  подключают общий заголовок; `neofetch` заодно лишился более неверного
  тега "(alpha)" у названия ОС
- Названо `nexus_version.h`, а не `version.h` — в `kernel/shell/apps/`
  уже есть свой `version.h` (заголовок команды `version`), одинаковое
  имя означало бы, что инклюд подхватывает не тот файл в зависимости
  от порядка путей поиска

## [0.4.3] — memoria

- Scrollback в консоли: PgUp/PgDn листают историю вывода
  (`drivers/console/console.c`, `drivers/keyboard/keyboard.c`).
  Кольцевой буфер на 500 строк (символ + цвет каждой ячейки), пишется
  параллельно с живым выводом без просадки скорости печати. Любая
  новая печать (набор текста, вывод команды) сама возвращает к живому
  виду
- PS/2 extended-scancode (префикс `0xE0`) — раньше драйвер клавиатуры
  его вообще не обрабатывал, из-за чего PgUp/PgDn (и любые другие
  extended-клавиши) молча терялись

## [0.4.2] — memoria

- Page fault handler (vector 14, `arch/x86_64/idt.c`) — расшифровка
  CR2 (адрес обращения) и error code (present/write/user/reserved/
  instruction-fetch) вместо общего `panic_screen()` без деталей

## [0.4.1] — memoria

- Свои page tables — `mm/paging.c`/`mm/paging.h`. 4-уровневая схема
  x86_64 (PML4/PDPT/PD), 2 MiB страницы, строит identity-map по
  EFI memory map из `boot_info` + framebuffer и реально переключает
  CR3 (раньше жили на identity-map, оставленной UEFI firmware —
  чужой формат, полагаться на который в принципе было нельзя)
- `meminfo` дополнен диагностикой paging (CR3, размер базовой
  identity-map)

## [0.4.0] — memoria

- Milestone 0.3 (refit) формально закрыт: живой бут в QEMU+OVMF
  подтверждён пользователем, встроенный шелл и команды проверены и
  работают
- `docs/STATUS.md`/`docs/ROADMAP.md` обновлены: снята оговорка "живой
  бут не подтверждён", следующая задача — page tables (PML4/PDPT/PD/PT),
  page fault handler, `kmalloc`/`kfree`
- Код в этой версии не менялся — только фиксация факта успешного
  бута и переход к следующему milestone

## [0.3.0] — refit

- **Архитектурный пивот**: BIOS/i386/GRUB Multiboot → UEFI/x86_64,
  см. `docs/adr/0002-uefi-x86_64-pivot.md`. Причина: старая ветка
  упёрлась в баг "no multiboot header found" и требовала болезненной
  сборки кросс-компилятора; у пользователя уже был более развитый
  рабочий проект на UEFI, перенесённый почти без изменения логики
  (только реорганизация путей — см. `docs/MIGRATION_0002.md`)
- Полный перенос: UEFI-загрузчик (GOP, ELF64-парсер, ExitBootServices),
  GDT/IDT/ISR под long mode (48 векторов), PIC/PIT, PS/2-клавиатура
  (полная инициализация i8042), консоль на framebuffer (шрифт 8x16),
  CPUID-обёртка, PCI+AHCI+FAT32, встроенный шелл с ~50 командами
- Новый Makefile — собирается обычным host `gcc`/`ld`, кросс-компилятор
  больше не нужен на x86_64 Linux
- Реальная сборка (не sanity-check) проверена в разработческой
  сессии: `BOOTX64.EFI` (валидный PE32+ EFI app) и `kernel.elf`
  (валидный ELF64, entry 0x200000) слинковались без единого warning'а.
  Живой бут в QEMU+OVMF не проверен — в песочнице разработки нет
  qemu-system-x86_64/OVMF/mkfs.vfat
- Удалены как неприменимые: `toolchain/build-cross-compiler.sh`,
  `config/grub.cfg`, весь `arch/i386/`-код, старый `kernel/kernel.h`
- ROADMAP.md переписан под фактическое состояние (архив/FS уже есть,
  paging/многозадачность/userspace — ещё нет, в другом порядке чем
  предполагал старый план)

## [0.1.2] — genesis

- Причина повторной коллизии `isr.o`/`irq.o` после обновления 0.1.1:
  распаковка нового архива поверх старой папки не удаляет файлы,
  которых нет в новом архиве — старые `arch/i386/isr.S`/`irq.S`
  оставались рядом с новыми `isr_stubs.S`/`irq_stubs.S`
- Makefile: добавлена явная проверка на коллизии путей `.o` перед
  сборкой (`DUPLICATE_OBJECTS`) — теперь такая ситуация даёт понятную
  ошибку сразу, а не загадочный "multiple definition" от линкера

## [0.1.1] — genesis

- Фикс: `arch/i386/isr.S` и `arch/i386/irq.S` переименованы в
  `isr_stubs.S`/`irq_stubs.S` — они собирались в те же объектники
  (`isr.o`/`irq.o`), что и одноимённые `isr.c`/`irq.c`, из-за чего
  линковщик ловил "multiple definition" и одновременно не находил
  `isr0..isr31`/`irq0..irq15` (реальный кросс-компилятор `i686-elf-gcc`
  вскрыл это на первой попытке `make iso` — sanity-check host-gcc'ом
  эту коллизию не поймал, потому что там объектники называлась иначе)

## [0.1.0] — genesis

- Multiboot boot-стаб (boot/boot.S), передача управления в kernel_main
- GDT: плоская модель, kernel/user code/data сегменты
- IDT + ISR: обработчики всех 32 исключений CPU с диагностикой
- IRQ: remap PIC на векторы 32-47, диспетчер обработчиков
- Драйверы: VGA text-mode, serial COM1, PS/2 keyboard, PIT timer
- kprintf (%d %u %x %s %c %%), вывод одновременно на VGA и serial
- PMM: bitmap physical memory manager по Multiboot memory map
- panic() — аварийная остановка с диагностикой
- Инфраструктура проекта: Makefile, ADR, ROADMAP, VERSIONING, STATUS
