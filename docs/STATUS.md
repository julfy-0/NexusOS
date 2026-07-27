# STATUS.md — читать первым

Единственный источник правды о текущем состоянии NexusOS. Обновляй
в конце каждой сессии — см. `docs/AI_HANDOFF.md`.

---

## Версия

**0.4.0-memoria** (см. `docs/VERSIONING.md`). Milestone 0.3 (refit)
закрыт: живой бут в QEMU+OVMF подтверждён, встроенный шелл и команды
проверены и работают. Предыдущий скачок 0.1.2 → 0.3.0 — замена
архитектуры целиком, см. `docs/adr/0002-uefi-x86_64-pivot.md`.

## Архитектура

- **UEFI** (свой загрузчик `boot/efi/`, PE32+, парсит ELF64 сам —
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
      полный список в `kernel/shell/apps/`)
- [x] `kstate` — глобальный доступ к boot_info (framebuffer, memory map)
      из любого места ядра

## Что НЕ сделано

- [ ] Виртуальная память / paging — сейчас identity map, оставленный
      UEFI. Своя MMU-настройка — следующий милстоун (см. ROADMAP)
- [ ] `kmalloc`/`kfree` (heap ядра)
- [ ] Многозадачность — всё выполняется синхронно в контексте
      прерывания клавиатуры
- [ ] User mode (ring 3), системные вызовы — шелл и команды это
      обычные C-функции в кольце 0, не изолированные процессы
- [ ] VFS абстракция шире FAT32 (уже есть vfs.c, но проверить глубину
      отдельно)

## Известные ограничения / долги

- Клавиатура — US QWERTY, Shift обрабатывается, Ctrl/Alt — нет
- Extras (`extras/c-practice/`) из старого проекта не перенесены —
  не относятся к ОС (личные C-упражнения автора)

## Следующая задача

Milestone 0.3 (refit) закрыт. Дальше — Milestone 0.4 "memoria"
(`docs/ROADMAP.md`):

1. Свои page tables (4-уровневая схема x86_64: PML4/PDPT/PD/PT) —
   сейчас identity-map, оставленный UEFI firmware
2. Page fault handler (vector 14) с осмысленной диагностикой
3. `kmalloc`/`kfree` — heap ядра на основе page allocator +
   физической memory map (уже приходит от UEFI через
   `nexus_boot_info_t.mmap`, см. `kstate_mem_summary()`)
4. Higher-half kernel (переезд с 0x200000) — не критично сразу, но
   нужно перед user/kernel split (Milestone 0.6)

## Правила для продолжающего

`docs/AI_HANDOFF.md` — обязательно перед новыми изменениями.
