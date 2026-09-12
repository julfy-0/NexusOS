# ROADMAP.md — путь развития NexusOS

После пивота на UEFI/x86_64 (`docs/adr/0002`) готовые куски не идут
строго по порядку старого плана genesis→memoria→threadwork→archive→
descent — импортированный проект принёс "archive" (диск+FAT32) раньше
"memoria" (paging) и "threadwork" (многозадачность). Ниже — актуальный
план от текущей точки, а не воображаемая линейная последовательность.


## NexusOS 0.5.2 → 0.6.0 master plan

This is the active development roadmap. The current rollback baseline is **0.5.3**;
0.5.3.6 is the active process-safe event integration milestone.

### 0.5.2 — Memory & execution foundation

- [x] 0.5.2.1 — Physical Memory Manager (PMM)
  - [x] Read UEFI memory map
  - [x] 4 KiB physical-page bitmap
  - [x] Reserve kernel/boot/framebuffer/low memory
  - [x] `pmm_alloc_page()` / `pmm_free_page()`
  - [x] `meminfo` PMM statistics
- [x] 0.5.2.2 — Virtual Memory Manager (VMM)
  - [x] 4 KiB page-table mapping API
  - [x] PMM-backed page-table allocation
  - [x] map/unmap/virtual-to-physical translation
  - [x] TLB invalidation after mapping changes
- [x] 0.5.2.3 — Kernel heap (`kmalloc`/`kfree`)
- [ ] 0.5.2.4 — Page-fault diagnostics
- [ ] 0.5.2.5 — Higher-half kernel

### 0.5.3 — Interrupts & scheduling

Rollback baseline: `0.5.3`; interrupt/event queue, scheduler, synchronization and kernel event waiting are now active.

- [x] 0.5.3.1 — Interrupt/event queue
  - [x] Fixed-size 256-event ring buffer
  - [x] IRQ capture-only handlers for PIT/PS2 keyboard/PS2 mouse
  - [x] Deferred keyboard and mouse processing in kernel context
  - [x] Deferred xHCI polling from timer IRQ to event loop
  - [x] Queue overflow statistics
  - [x] Shell/GUI execution removed from PS/2 IRQ context
- [x] 0.5.3.2 — Timer-driven scheduler foundation
- [x] 0.5.3.3 — Threads & TCB
- [x] 0.5.3.4 — Scheduler ready queue and sleep/wakeup
  - [x] Intrusive FIFO ready queue
  - [x] Ordered sleep queue by wake tick
  - [x] `thread_sleep_ms()`
  - [x] `thread_wakeup()`
  - [x] Deferred timer-driven wakeups outside IRQ context
  - [x] Zombie stack/TCB reclamation
  - [x] Ready/sleeping scheduler diagnostics
- [x] Threads and TCB
- [x] x86_64 context switching
- [x] Move shell execution out of IRQ context
- [x] 0.5.3.5 — Synchronization primitives
- [x] 0.5.3.6 — Process-safe event/wait integration
  - [x] Per-event-type sequence counters
  - [x] Kernel thread event wait queues
  - [x] Race-safe event wait/check boundary
  - [x] Broadcast wakeup of event waiters
  - [x] No scheduler mutation or context switching from IRQ handlers

### 0.5.4 — Processes & user execution

- [ ] Process/address-space abstraction
- [ ] TSS and ring-3 transition
- [ ] User/kernel memory permissions
- [ ] Syscall ABI
- [ ] ELF user loader
- [ ] `init` process

### 0.5.5 — Real shell & filesystem I/O

- [ ] User-space shell process
- [ ] Robust argument parser and quoting
- [ ] Escape sequences
- [ ] `>`, `>>`, `<` redirection
- [ ] Pipelines (`|`)
- [ ] `&&` and `;`
- [ ] Exit/status codes
- [ ] File descriptors
- [ ] Writable FAT32 integration

### 0.5.6 — Unified input & USB HID

- [ ] USB HID abstraction
- [ ] USB keyboard input
- [ ] USB mouse input
- [ ] Unified PS/2 + USB input API
- [ ] Event-based GUI input

### 0.5.7 — GUI / Window System 2.0

- [ ] Window abstraction
- [ ] Window bounds/title/focus/visibility
- [ ] Active window management
- [ ] Close button/events
- [ ] Desktop integration
- [ ] Application launching foundation

### 0.5.8 — Nexus system services

- [ ] `init` service/session architecture
- [ ] App registry
- [ ] `.nx` package specification
- [ ] `manifest.nxm` parser
- [ ] Basic Package Manager discovery/metadata
- [ ] App Manager foundation
- [ ] `/system/apps` and `/userdata/apps` integration
- [ ] Font abstraction while retaining bitmap font
- [ ] Asset abstraction

### 0.5.9 — Networking & security foundation

- [ ] Ethernet/NIC foundation
- [ ] ARP
- [ ] IPv4
- [ ] ICMP
- [ ] UDP
- [ ] TCP foundation
- [ ] User/kernel isolation hardening
- [ ] Process/file permissions foundation
- [ ] Syscall validation

### 0.6.0 — Integration release

- [ ] All previous milestones integrated
- [ ] Stable boot flow
- [ ] PMM/VMM/heap operational
- [ ] Scheduler operational
- [ ] Ring 3 operational
- [ ] Syscalls operational
- [ ] ELF applications operational
- [ ] `init` operational
- [ ] Shell runs as user-space application
- [ ] File descriptors and persistent filesystem I/O operational
- [ ] Unified input operational
- [ ] Window system foundation operational
- [ ] Package/App Manager foundation operational
- [ ] Networking foundation operational
- [ ] Security boundaries validated
- [ ] Build, ISO/image and regression checks pass

### Development rule

Each `x.x.x` block is closed only when its implementation and validation are
complete. At that point the block receives a GitHub Release description.

## Milestone 0.3 — refit (закрыт) — UEFI/x86_64 пивот

- [x] Перенос всего кода в новую структуру, реальная сборка проверена
- [x] **Живой бут в QEMU+OVMF подтверждён** — шелл и команды проверены

## Milestone 0.4 — memoria (Virtual Memory)

## Milestone 0.5 — Enstein (VFS path traversal) — текущий

Сейчас работает identity-map, оставленный UEFI firmware — это не
"настоящая" виртуальная память, а просто то, что было до нас.

- [ ] Свои page tables (4-уровневая схема x86_64: PML4/PDPT/PD/PT)
- [ ] Higher-half kernel (переезд с 0x200000 на что-то вроде
      0xFFFFFFFF80000000) — сейчас не критично, но нужно перед
      настоящим user/kernel split
- [ ] Page fault handler (vector 14) с осмысленной диагностикой —
      сейчас все исключения 0-31 просто ведут в panic_screen()
- [ ] `kmalloc`/`kfree` — heap ядра на основе page allocator +
      реальной физической memory map (она уже приходит от UEFI
      через `nexus_boot_info_t.mmap`, см. `kstate_mem_summary()`
      в `kernel/core/kstate.c` — уже читает её, просто не строит из этого
      allocator)

## Milestone 0.5 — threadwork (Многозадачность)

- [ ] `task_t`/TCB, переключение контекста (context switch для x86_64
      — сохранение регистров, RSP, CR3 после появления paging)
- [ ] Планировщик — сейчас всё однопоточное и синхронное
      (шелл живёт прямо в обработчике IRQ1, см. предупреждение в
      шапке `shell/shell.c`)
- [ ] Разорвать связь "клавиатура → напрямую shell_input_char()" —
      после появления многозадачности это должно идти через очередь
      событий, а не звонить в шелл прямо из контекста прерывания

## Milestone 0.6 — descent (User Mode)

- [ ] TSS, переход в ring 3
- [ ] Системные вызовы (syscall/sysret — уже в long mode, естественный
      выбор вместо int 0x80)
- [ ] Вынести шелл + 50 команд из kernel-context в user-space процесс —
      технически это единственная реализация шелла, которая есть,
      просто она сейчас работает "неправильно" (в кольце 0); не
      писать заново, перенести
- [ ] Минимальный libc для user-space

## Storage/FS — уже частично готово, но не оформлено как отдельный
## milestone, потому что пришло "бесплатно" вместе с пивотом

- [x] PCI enumeration, AHCI (SATA)
- [x] FAT32: монтирование, чтение (`fs/fat32.c`)
- [ ] Запись в FAT32 — проверить, реализована ли, или только чтение
- [ ] VFS слой (`shell/apps/vfs.c` существует — проверить,
      насколько это настоящая абстракция или просто обёртка над FAT32)

## Дальше (после descent)

- Сеть, SMP, журналируемая ФС — как и раньше, не расписано подробно

## Как решаем, что "готово" на 1.0

Не по чекбоксам — отдельным ADR, когда реально самодостаточна:
грузится, многозадачность, ФС, user-space процессы, интерактивный
шелл (уже есть, но должен переехать в user-space).

## Graphical shell + user mode foundation
- Graphical framebuffer desktop with centered Nexus OS identity and black theme.
- Application surface: Files / Terminal / Settings cards; window manager and mouse are next.
- Ring-3 foundation: user code/data selectors and 64-bit TSS with rsp0.
- Next: 4 KiB user pages with U/S permission, syscall entry, process object, scheduler, ELF user loader.
