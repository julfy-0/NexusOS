# STATUS.md — читать первым

Единственный источник правды о текущем состоянии NexusOS. Обновляй
в конце каждой сессии — см. `docs/AI_HANDOFF.md`.

---

## Версия

**0.5.1 — Desktop Update** (см. `docs/VERSIONING.md`). Milestone 0.3 (refit)
закрыт: живой бут в QEMU+OVMF подтверждён, встроенный шелл и команды
проверены и работают. Предыдущий скачок 0.1.2 → 0.3.0 — замена
архитектуры целиком, см. `docs/adr/0002-uefi-x86_64-pivot.md`.

Версия теперь также зашита в код: `include/nexus/nexus_version.h` —
единственное место в коде, откуда её берут команды `version`/`neofetch`/
`uname` (раньше каждая хардкодила свою несовпадающую строку).


### 0.5.1 Desktop Update

- [x] Graphical desktop launched with `desktop-run`
- [x] Files, Terminal and Settings GUI applications
- [x] Nexus Menu and Desktop Search
- [x] Keyboard and PS/2 mouse navigation in the GUI
- [x] Floating responsive dock and final GUI polish
- [x] Release build verified with `make clean && make iso`

## Организация исходников

- `boot/uefi/` — собственный UEFI-загрузчик, разделённый на `src/`, `include/` и `assets/`
- `kernel/core/` — точка входа ядра, состояние ядра, panic и usermode foundation
- `kernel/arch/x86_64/` — архитектурно-зависимые GDT/IDT/ISR/entry/linker
- `kernel/mm/` — собственный paging
- `drivers/` — устройства, сгруппированные по назначению (input, storage, bus, graphics, timer, hardware, usb)
- `fs/vfs/` — VFS core, mount namespace и filesystem registry; `fs/fat32/` — FAT32
- `gui/` — GUI core и renderer/font boundary
- `shell/` — shell core и команды по категориям
- `assets/` — заменяемые wallpapers/fonts
- `include/nexus/` — общие заголовки NexusOS

Это реорганизация существующего кода, а не новая архитектура ядра: монолитная модель и текущие runtime-зависимости сохранены.

## Архитектура

- **UEFI** (свой загрузчик `boot/uefi/`, PE32+, парсит ELF64 сам —
  без GRUB), не BIOS
- **x86_64 long mode**, не i386
- **Монолитное ядро** (не изменилось, см. `docs/adr/0001`) — шелл
  и все команды выполняются в контексте ядра, не user-space
- Собирается **обычным host gcc/ld** — кросс-компилятор не нужен,
  если разработка идёт на x86_64 Linux

## Что уже работает (собрано, слинковано и **живьём протестировано**
## в QEMU+OVMF пользователем — не sanity-check, реальный бут
## подтверждён, встроенный шелл и команды проверены и работают)

- [x] **Живой бут в QEMU+OVMF подтверждён** — грузится, шелл отвечает
- [x] UEFI-загрузчик: GOP framebuffer, чтение kernel.elf с ESP,
      парсинг ELF64 + раскладка PT_LOAD, ExitBootServices, передача
      управления с `nexus_boot_info_t*`
- [x] GDT/IDT/ISR под long mode, 48 векторов (32 исключения + 16 IRQ)
- [x] PIC remap, PIT timer, PS/2 keyboard (полная инициализация
      контроллера i8042, не просто чтение порта)
- [x] Консоль на линейном framebuffer, битмап-шрифт 8x16
- [x] CPUID-обёртка (vendor/brand string/логические ядра)
- [x] PCI enumeration, AHCI (SATA), FAT32 (монтирование, чтение)
- [x] Встроенный шелл + **~50 команд** (ls/cat/cp/mv/grep/find/diff/
      wc/df/du/calc/hex/dec/neofetch/sysinfo/reboot/halt/... —
      полный список в `shell/apps/`)
- [x] `kstate` — глобальный доступ к boot_info (framebuffer, memory map)
      из любого места ядра
- [x] Свои page tables (`kernel/mm/paging.c`) — 4-уровневая схема PML4/PDPT/PD,
      2 MiB страницы, реально переключает CR3 (не identity-map от UEFI)
- [x] Higher-half execution — low bootstrap trampoline + high kernel alias
- [x] Page fault handler (vector 14) — расшифровка CR2/error code
      (present/write/user/reserved/instruction-fetch)
- [x] Scrollback в консоли (PgUp/PgDn) — кольцевой буфер истории строк
      в `drivers/graphics/framebuffer/console.c`, живой вывод не замедляет
- [x] `neofetch` расширен: измеренная частота CPU (калибровка TSC по
      PIT, `cpu_measure_freq_mhz()`), пользователь, uptime — не только
      vendor/brand/cores/память, как раньше

## Что НЕ сделано

- [x] `kmalloc`/`kfree` — kernel heap на PMM/VMM, 16-byte alignment
- [x] Higher-half kernel — kernel VMA `0xFFFFFFFF80000000`, physical load base `0x00200000`
- [ ] Многозадачность — всё выполняется синхронно в контексте
      прерывания клавиатуры
- [ ] User mode (ring 3), системные вызовы — шелл и команды это
      обычные C-функции в кольце 0, не изолированные процессы
- [ ] VFS абстракция шире FAT32 (уже есть vfs.c, но проверить глубину
      отдельно)

## Известные ограничения / долги

- `neofetch`/`meminfo` показывают память из статичного снимка EFI
  memory map с момента бута, не живой трекинг аллокаций — `kmalloc`
  ещё не существует, значит и трекать пока нечего. Как появится heap —
  заменить на честную живую статистику аллокатора (см. комментарий в
  `shell/apps/neofetch.c`).
- `neofetch` блокирует на ~150 мс на калибровку CPU-частоты (busy-wait
  на PIT) — ожидаемо, не баг.
- Клавиатура — US QWERTY, Shift обрабатывается, Ctrl/Alt — нет
- Extended-клавиши (стрелки, Home/End) кроме PgUp/PgDn пока
  игнорируются драйвером — не наша задача сейчас
- Extras (`extras/c-practice/`) из старого проекта не перенесены —
  не относятся к ОС (личные C-упражнения автора)

## Следующая задача

**0.5.3 — Interrupts & scheduling:** event queue, timer-driven scheduler,
TCB/context switching, moving shell execution out of IRQ context, and basic
synchronization primitives.

## Правила для продолжающего

`docs/AI_HANDOFF.md` — обязательно перед новыми изменениями.
