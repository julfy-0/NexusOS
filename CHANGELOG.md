# Changelog

## 0.5.2 — System Foundation

- Added the Nexus System Core and explicit runtime states.
- Added GPT partition discovery and multi-context FAT32 mounts.
- Added real BOOT/SYSTEM/USERDATA GPT image generation.
- Moved kernel loading to SYSTEM with a safe legacy BOOT fallback.
- Added session, power, system-info, package-manager and app-manager foundations.
- Added `.nx` package manifest format foundation without fake executable loading.
- Preserved the existing kernel, bootloader, CLI, GUI, input, VFS, FAT32 and AHCI paths.

# CHANGELOG.md

## [0.5.1] — Desktop Update

### Architecture maintenance
- Reorganized the existing source tree by subsystem without changing the kernel model or runtime behavior.
- Moved the UEFI loader into `boot/uefi/{src,include,assets}` and x86_64 kernel architecture code into `kernel/arch/x86_64`.
- Separated kernel core, memory management, VFS core/mount/registry, GUI core/renderer, shell core/commands, and driver categories.
- Moved the existing bitmap font and wallpaper into replaceable shared asset directories.
- Reworked the Makefile to mirror source paths under `build/`, avoiding object-name collisions and making new modules discoverable automatically.
- Updated `build.sh` to 0.5.1, retained parallel GNU Make builds, and kept `--no-clean` support.
- No release-version bump: this remains the 0.5.1 architecture optimization.


### Desktop
- Added the new NexusOS graphical desktop with wallpaper, floating dock, application icons, clock and active application labels.
- Added keyboard and mouse navigation across the desktop UI.
- Added hover states and edge-detected mouse clicks.

### Applications
- Added graphical Files with VFS-backed navigation.
- Added graphical Terminal with interactive command input.
- Added graphical Settings with system information.
- Added Nexus Menu with Desktop, Files, Terminal, Settings, Reboot and Shutdown actions.

### Search
- Added Desktop Search for applications and system actions.
- Search supports text input, Backspace, keyboard selection, Enter and Escape.

### Integration
- GUI starts through the existing `desktop-run` command and returns to the CLI when closed.
- Preserved the existing UEFI bootloader, CLI, VFS, paging, GDT/IDT/PIC/PIT, Kernel Panic, PS/2 keyboard and mouse drivers.
- Added `build.sh` parallel build frontend with Kernel, Drivers, Bootloader and overall OS progress.

### Release validation
- Verified with `make clean && make iso`.
- UEFI bootloader and kernel build successfully and the ESP layout is generated.


## [0.5.0] — Enstein

- Реализована первая рабочая прослойка VFS path traversal поверх mount namespace.
- `/mnt` и `/mnt/disk0` теперь являются постоянными namespace-узлами VFS.
- `cd`, `pwd`, `ls` и `cat` умеют переходить в смонтированный FAT32 и работать с путями внутри него.
- Добавлена проверка FAT32-директорий для `cd`.
- Операции записи в RAM VFS блокируются внутри read-only FAT32 mount, чтобы не создавать ложные RAM-файлы поверх диска.
- Версия NexusOS повышена до `0.5.0-Enstein`.

Формат: `## [версия] — codename` + список изменений. Новое — сверху.

## [0.4.6] — memoria

- `neofetch` расширен тремя новыми полями:
  - **CPU Speed** — реальное измерение частоты через калибровку TSC по
    PIT (`cpu_measure_freq_mhz()`, `drivers/hardware/cpu/cpu.c`), а не чтение
    статического поля CPUID leaf 0x16 (тот часто не реализован
    гипервизорами, включая QEMU/TCG — были бы нули или мусор что на
    реальном железе, что в виртуалке в непредсказуемых случаях).
    Блокирует на ~150 мс (busy-wait на PIT) — ожидаемо.
  - **User** — `root@nexusos` (системы без аккаунтов, как в `whoami`
    — единообразно, не выдумано заново)
  - **Uptime** — через уже существующий `pit_get_uptime_seconds()`
    (то же, что в команде `uptime`)
- `drivers/timer/pit.h`/`.c`: новый геттер `pit_get_frequency_hz()` —
  нужен `cpu_measure_freq_mhz()` как опорная частота для калибровки
- Метка "Memory" в `neofetch` уточнена до "free (at boot)" — это
  честная формулировка: цифра пересчитывается на каждый вызов, но
  источник — статичный EFI-снимок с бута, не живой трекинг аллокаций
  (kmalloc ещё не существует, см. `docs/STATUS.md`)

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
- Названо `nexus_version.h`, а не `version.h` — в `shell/apps/`
  уже есть свой `version.h` (заголовок команды `version`), одинаковое
  имя означало бы, что инклюд подхватывает не тот файл в зависимости
  от порядка путей поиска

## [0.4.3] — memoria

- Scrollback в консоли: PgUp/PgDn листают историю вывода
  (`drivers/graphics/framebuffer/console.c`, `drivers/input/keyboard/keyboard.c`).
  Кольцевой буфер на 500 строк (символ + цвет каждой ячейки), пишется
  параллельно с живым выводом без просадки скорости печати. Любая
  новая печать (набор текста, вывод команды) сама возвращает к живому
  виду
- PS/2 extended-scancode (префикс `0xE0`) — раньше драйвер клавиатуры
  его вообще не обрабатывал, из-за чего PgUp/PgDn (и любые другие
  extended-клавиши) молча терялись

## [0.4.2] — memoria

- Page fault handler (vector 14, `kernel/arch/x86_64/idt.c`) — расшифровка
  CR2 (адрес обращения) и error code (present/write/user/reserved/
  instruction-fetch) вместо общего `panic_screen()` без деталей

## [0.4.1] — memoria

- Свои page tables — `kernel/mm/paging.c`/`kernel/mm/paging.h`. 4-уровневая схема
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
