# ROADMAP.md — путь развития NexusOS

После пивота на UEFI/x86_64 (`docs/adr/0002`) готовые куски не идут
строго по порядку старого плана genesis→memoria→threadwork→archive→
descent — импортированный проект принёс "archive" (диск+FAT32) раньше
"memoria" (paging) и "threadwork" (многозадачность). Ниже — актуальный
план от текущей точки, а не воображаемая линейная последовательность.


## NexusOS 0.5.2 → 0.6.0 master plan

This is the active development roadmap. The current release remains **0.5.1**;
0.5.2 is the next development milestone.

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
- [x] 0.5.2.4 — Page-fault diagnostics
  - [x] Dedicated #PF handler
  - [x] CR2 and error-code decoding
  - [x] Access/protection/privilege diagnostics
  - [x] RIP/CS/RFLAGS/CR3 reporting
  - [x] Safe non-recursive diagnostic path
- [x] 0.5.2.5 — Higher-half kernel
  - [x] Separate physical load address and higher-half virtual address
  - [x] Low physical bootstrap trampoline
  - [x] Temporary identity + higher-half bootstrap mappings
  - [x] Higher-half kernel execution entry
  - [x] Final page tables retain higher-half kernel mapping
  - [x] PMM reserves physical kernel range correctly

### 0.5.3 — Interrupts & scheduling

- [ ] Interrupt/event queue
- [ ] Timer-driven scheduler
- [ ] Threads and TCB
- [ ] x86_64 context switching
- [ ] Move shell execution out of IRQ context
- [ ] Synchronization primitives

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

## Historical milestone notes

The older milestone labels below are retained for history; the active
implementation status is tracked in the `0.5.2 → 0.6.0 master plan` above.

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
