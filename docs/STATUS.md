# STATUS.md — читать первым

Единственный источник правды о текущем состоянии NexusOS. Обновляй
в конце каждой сессии — см. `docs/AI_HANDOFF.md`.

## Версия

**0.5.1 — Desktop Update.** Текущая архивная/рабочая версия не меняется
этой архитектурной чисткой. Работа в этом состоянии подготавливает дерево
для будущего 0.5.2; официальная версия проекта остаётся 0.5.1.

Версия в коде хранится в `include/nexus/nexus_version.h` и используется
командами `version`, `neofetch`, `uname` и GUI.

### 0.5.1 Desktop Update

- [x] Graphical desktop launched with `desktop-run`
- [x] Files, Terminal and Settings GUI applications
- [x] Nexus Menu and Desktop Search
- [x] Keyboard and PS/2 mouse navigation in the GUI
- [x] Floating responsive dock and GUI polish
- [x] Release build verified with `make clean && make iso`
- [x] GUI split into core/state, renderer, desktop, input, search and app modules
- [x] Shell command routing moved into a command registry
- [x] FAT32 implementation grouped under `fs/fat32/`

## Организация исходников

- `boot/uefi/` — собственный UEFI-загрузчик, разделённый на `src/`,
  `include/` и `assets/`
- `kernel/core/` — точка входа ядра, состояние, panic и usermode foundation
- `kernel/arch/x86_64/` — архитектурно-зависимые GDT/IDT/ISR/entry/linker
- `kernel/mm/` — собственный paging
- `drivers/` — устройства, сгруппированные по назначению
- `fs/vfs/` — VFS core, mount namespace и registry; `fs/fat32/` — FAT32
- `gui/` — lifecycle/state, desktop, input, renderer, search и приложения
- `shell/` — shell core, command registry и команды по категориям
- `assets/` — заменяемые wallpapers/fonts
- `include/nexus/` — общие заголовки NexusOS

Это реорганизация существующего кода, а не новая архитектура ядра:
монолитная модель и текущие runtime-зависимости сохранены.

## Что уже работает

- [x] **Живой бут в QEMU+OVMF подтверждён пользователем** — грузится, шелл отвечает
- [x] UEFI-загрузчик: GOP framebuffer, чтение kernel.elf с ESP,
      ELF64 + PT_LOAD, ExitBootServices и `nexus_boot_info_t`
- [x] GDT/IDT/ISR под long mode, 48 векторов
- [x] PIC remap, PIT timer, PS/2 keyboard и PS/2 mouse
- [x] Framebuffer console, bitmap font 8x16
- [x] CPUID-обёртка и hardware inventory
- [x] PCI enumeration, AHCI (SATA), FAT32 (монтирование, чтение)
- [x] Встроенный шелл с существующим набором системных, файловых и utility-команд
- [x] `kstate` — глобальный доступ к `boot_info`
- [x] Собственные page tables и переключение CR3
- [x] Page fault handler с CR2/error-code диагностикой
- [x] Scrollback PgUp/PgDn
- [x] `neofetch` с измеренной частотой CPU, user и uptime
- [x] GUI с desktop, dock, search, Files, Terminal, Settings
- [x] GUI-модули имеют отдельные границы состояния/рендера/input/apps
- [x] Shell command registry сохраняет существующие команды и dispatch

## Что НЕ сделано

- [ ] `kmalloc`/`kfree` (heap ядра)
- [ ] Higher-half kernel
- [ ] Многозадачность
- [ ] User mode (ring 3) и системные вызовы
- [ ] Более глубокая VFS abstraction поверх нескольких реальных ФС
- [ ] Полноценная window manager/compositor система
- [ ] Настоящий runtime font loader / TTF parser

## Известные ограничения / долги

- `neofetch`/`meminfo` используют информацию EFI memory map со старта; живого
  heap tracking пока нет.
- `neofetch` блокирует примерно на 150 мс при калибровке TSC/PIT.
- Клавиатура — US QWERTY, Shift обрабатывается, Ctrl/Alt ограничены.
- Extended keyboard support остаётся минимальным.
- `kernel/bootmode/` сохранён как legacy source и не входит в default link.
- GUI-приложения пока работают как один kernel-side GUI runtime, без процессов.

## Следующая задача

Подготовка к 0.5.2 завершена на уровне структуры. Следующий функциональный
milestone остаётся memory/heap work:

1. `kmalloc`/`kfree` на основе доступной физической memory map
2. Higher-half kernel
3. После этого — scheduler/context switching и затем user/kernel split

Не добавлять heap, scheduler или ring-3 только ради этой структурной чистки.

## Правила для продолжающего

`docs/AI_HANDOFF.md` — обязательно перед новыми изменениями.
