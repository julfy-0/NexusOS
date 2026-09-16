### 0.5.36 — USB controller stability and keyboard safety

- [x] Harden xHCI BIOS/OS ownership handoff with a bounded timeout
- [x] Wait for xHCI Controller Not Ready (CNR) to clear after reset
- [x] Abort cleanly when an xHCI controller cannot reach the running state
- [x] Bound xHCI event processing per normal-context pass
- [x] Preserve PS/2 keyboard operation when USB controller initialization fails
- [x] Keep multi-controller xHCI probing and fallback behavior
- [ ] Full USB hotplug/device-class manager

# ROADMAP.md — путь развития NexusOS

## 0.5.35 — Colored core and service startup logs

- Apply the compact startup log format to core initialization and services.
- Add color-coded component names and status results.
- Report GDT, IDT, kernel address space, PMM, VMM, virtual arena, allocator, event queue and scheduler startup.
- Preserve short `[ OK ]` / `[ FAILED ]` boot output.


После пивота на UEFI/x86_64 (`docs/adr/0002`) готовые куски не идут
строго по порядку старого плана genesis→memoria→threadwork→archive→
descent — импортированный проект принёс "archive" (диск+FAT32) раньше
"memoria" (paging) и "threadwork" (многозадачность). Ниже — актуальный
план от текущей точки, а не воображаемая линейная последовательность.


## NexusOS 0.5.2 → 0.6.0 master plan

This is the active development roadmap. The current release baseline is **0.5.28 — Enstein**;
0.5.4 — Processes & Userspace Foundation is complete as a foundation milestone.
0.5.5 — Shell & I/O Foundation is integrated.
0.5.6 — Unified Input & USB HID is integrated.
0.5.7 — GUI / Window System 2.0 is complete.
0.5.10 — App Manager Foundation and 0.5.12 — Application Discovery are completed milestones.

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

### 0.5.4 — Processes & Userspace Foundation

- [x] 0.5.4.1 — Process & address-space foundation
  - [x] Fixed process table and stable PID allocation
  - [x] Process lifecycle states and current-process metadata
  - [x] Future CR3 ownership metadata
  - [x] Reserved user virtual-range metadata
- [x] 0.5.4.2 — TSS and ring-3 transition foundation
  - [x] Ring-3 GDT selectors and TSS.RSP0 contract
  - [x] IRETQ transition trampoline
  - [x] Per-process user execution context metadata
- [ ] User/kernel memory permissions
- [ ] Syscall ABI
- [ ] ELF user loader
- [x] 0.5.4 — User/Kernel Memory Permissions & Address Space Activation
  - [x] PMM-backed 4 KiB user pages mapped with U/S permissions
  - [x] Per-process user mapping ownership and cleanup
  - [x] Active CR3 recorded explicitly (private CR3 remains future work)

- [ ] `init` process

### 0.5.5 — Shell & filesystem I/O

- [x] Robust argument parser and quoting
- [x] Escape sequences
- [x] `&&` and `;`
- [x] Parser recognition for `>`, `>>`, `<` and `|`

Pipelines and redirection now execute through bounded console-output capture and
the existing VFS. File descriptors and persistent writable FAT32 remain future work.

### 0.5.5 remaining

- [ ] User-space shell process
- [x] Robust argument parser and quoting
- [x] Escape sequences
- [x] `>`, `>>`, `<` redirection through VFS
- [x] Pipelines (`|`) through bounded shell I/O transport
- [x] `&&` and `;`
- [x] Shell command success/failure status for control flow
- [ ] Per-process file descriptors
- [ ] Writable persistent FAT32 integration

### 0.5.6 — Unified input & USB HID

- [x] USB HID boot abstraction on xHCI
- [x] USB keyboard input
- [x] USB mouse input
- [x] Unified PS/2 + USB input API
- [x] Event-based GUI input remains deferred to normal kernel context

### 0.5.7 — GUI / Window System 2.0

- [x] Window abstraction
- [x] Window bounds/title/focus/visibility
- [x] Active window management
- [x] Close button/events
- [x] Mouse title-bar dragging
- [x] Desktop integration
- [x] Application launching foundation

### 0.5.9 — Nexus system services

- [x] `init` service/session architecture foundation
- [x] App registry
- [x] `.nx` package specification foundation
- [x] `manifest.nxm` parser
- [x] Basic Package Manager metadata foundation
- [ ] App Manager foundation
- [ ] `/system/apps` and `/userdata/apps` integration
- [ ] Font abstraction while retaining bitmap font
- [ ] Asset abstraction

### 0.5.10 — App Manager Foundation

- [x] Central application registry
- [x] Stable application IDs and descriptors
- [x] Built-in application registration API
- [x] Application source classification (`builtin`, `system`, `userdata`)
- [x] Application lifecycle state (`stopped` / `running`)
- [x] Active application tracking
- [x] Launch counters and runtime inspection
- [x] `/system/apps` read-only discovery
- [x] `/userdata/apps` read-only discovery
- [ ] Writable package installation
- [ ] ELF/user-space application execution

### 0.5.12 — Writable FAT32 Foundation

- [x] AHCI `WRITE DMA EXT` sector write path
- [x] FAT32 cluster allocation and freeing
- [x] Mirrored FAT updates
- [x] 8.3 file creation and overwrite
- [x] 8.3 directory creation
- [x] Writable FAT32 mount at `/mnt/disk0`
- [x] VFS routing for FAT32 `mkdir`, `touch`, and `write`
- [x] Package installation transaction
- [ ] ELF/user-space application execution

### 0.5.13 — Package Installation Transaction

- [x] Package manifest validation before mutation
- [x] Staged application directory creation
- [x] Bounded Entry payload copy
- [x] Manifest-last publication/commit point
- [x] Rollback of newly created payload and directory on failure
- [x] Immediate post-install application discovery
- [ ] Multi-file package extraction
- [ ] Long filename package support
- [x] ELF/user-space loader foundation

### 0.5.15 — ELF64 user-space loader foundation

- [x] ELF64 header/program-header validation for x86_64
- [x] `PT_LOAD` image loading with zero-filled BSS
- [x] Per-page user permissions (`U/S`, writable, NX)
- [x] Fixed-size user image + stack page ownership
- [x] ET_EXEC and ET_DYN load support
- [x] `elf-run <path>` synchronous Ring-3 execution path
- [x] Syscall entry/return path
- [ ] Scheduler-owned user processes
- [ ] Private CR3 per process

### 0.5.23 — Safe userspace memory access

- [x] Add kernel-to-userspace read/write access helpers with explicit range validation
- [x] Require writable user mappings for kernel writes into userspace
- [x] Reject userspace memory access for zombie/invalid processes
- [x] Reuse private CR3 translation for bounded cross-address-space copies
- [x] Preserve the existing ELF loader write path for non-writable executable mappings

### 0.5.21 — Syscall validation and Ring-3 ABI hardening

- [x] Validate syscall caller process ownership and scheduler TCB association
- [x] Validate Ring-3 return-frame selectors and RFLAGS policy
- [x] Validate syscall RIP against mapped executable userspace memory
- [x] Validate user RSP against mapped writable userspace memory
- [x] Reject syscall dispatch when no valid scheduler-owned user process is active
- [x] Preserve non-blocking/capture-only IRQ architecture

### 0.5.20 — Process lifecycle and safe reaping

- [x] Separate process termination from resource destruction
- [x] Introduce `PROCESS_ZOMBIE` lifecycle state
- [x] Preserve private CR3/user memory until the scheduler leaves the process
- [x] Record process exit code/reason metadata
- [x] Scheduler-owned process reaping after TCB hand-off
- [x] Prevent PID/slot reuse while a process remains a zombie
- [x] Keep kernel-mode exceptions fatal and user page faults process-local

### 0.5.19 — User page-fault isolation

- [x] Ring-3 page-fault detection using the CPL in the saved CS
- [x] CR2/error-code diagnostics
- [x] Faulting scheduler-owned user process termination
- [x] Return to scheduler instead of global Kernel Panic for user page faults
- [x] Kernel-mode page faults remain fatal

### 0.5.17 — Syscall entry/return foundation

- [x] DPL3 `INT 0x80` syscall gate
- [x] x86_64 syscall entry stub preserving all GPRs
- [x] User return-frame validation
- [x] `NOP` syscall
- [x] `GETPID` syscall
- [ ] Scheduler-owned user process termination
- [ ] `EXIT` syscall completion

### 0.5.14 — Multi-file package payload

- [x] Manifest `Files` list for multiple package payloads
- [x] 8.3 payload-name validation
- [x] Preflight validation of every payload before destination mutation
- [x] Multi-file staged copy with rollback
- [x] Manifest-last publication remains the commit point
- [ ] Long filename package support
- [ ] ELF/user-space execution

### 0.5.9 — Networking & security foundation

- [x] Ethernet/NIC foundation (Intel E1000 detection, PCI/MMIO binding, MAC discovery)
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



### 0.5.23 — Userspace console output syscall

- [x] Bounded `WRITE` syscall for stdout/stderr
- [x] Validate complete userspace buffer before access
- [x] Copy userspace data through private CR3 helpers
- [x] Non-blocking console output path
- [ ] File-descriptor-backed userspace I/O

### 0.5.28 — USB controller compatibility and graphics performance

- [x] Harden USB PCI controller probing and diagnostics
- [x] Report discovered PCI USB controllers when initialization fails
- [x] Explicitly map EHCI/OHCI MMIO regions before register access
- [x] Keep xHCI as the preferred controller backend
- [x] Replace per-pixel GUI framebuffer primitives with row-based fast paths
- [x] Add cached nearest-neighbor wallpaper coordinate maps
- [x] Preserve and restore the cursor without redrawing the full desktop on every mouse move
- [x] Keep UEFI GOP framebuffer as the active display backend for real hardware stability
- [ ] Vendor-specific GPU command submission / hardware acceleration

### 0.5.24 — Per-process file descriptor foundation

- [x] Fixed-size per-process file descriptor table
- [x] Standard descriptors 0/1/2 initialized for every process
- [x] Descriptor type tracking for future VFS-backed handles
- [x] Route userspace `WRITE` through the process descriptor table
- [x] Add `CLOSE` syscall for process-owned descriptors
- [x] Reset descriptor state during process reaping
- [ ] VFS/FAT32 file descriptor objects
- [ ] Blocking `READ` and stdin event delivery

### 0.5.18 — Private process address spaces

- [x] Private CR3 allocated per process
- [x] Private page-table hierarchy cloned from the kernel address space
- [x] 4 KiB user mappings installed in the owning process CR3
- [x] Scheduler CR3 switch coupled to process TCB ownership
- [x] User ELF image/stack initialization through process translations
- [ ] Copy-on-write / shared memory
- [ ] Page-fault-driven demand mapping

### 0.5.34 — Concise driver and service startup logs
- Standardized startup lines to name, type, version and result.
- Driver/service initialization reports `[ OK  ]` or `[ FAILED ]`.
- Removed redundant verbose module startup summaries from the boot path.

### 0.5.33 — Terminal scrollback shortcuts
- Add Ctrl+Up/Ctrl+Down shortcuts for terminal scrollback.
- Preserve Up/Down shell command history when Ctrl is not pressed.
- Support the shortcut path for both PS/2 and USB HID keyboards.
- Keep scrollback changes out of IRQ context by handling them in normal input processing.

### 0.5.32 — Terminal text cursor

- [x] Blinking text cursor in the command-line shell
- [x] Cursor follows the active input position
- [x] Cursor hides during shell redraw/output and graphical desktop mode
- [x] PIT-driven cursor blink timing

### 0.5.31 — Kernel driver modules foundation

- [x] Add kernel module descriptor ABI for built-in drivers
- [x] Add `.nexus_modules` linker section and module discovery
- [x] Add priority-ordered driver module loading
- [x] Add module runtime state and load diagnostics
- [x] Move PCI/PIC/PIT/input/keyboard/mouse/network/NVMe/AHCI/USB/GPU initialization behind modules
- [ ] External `.mod`/`.ko` runtime loading from filesystem
- [ ] Module dependency graph and unload support

### 0.5.29 — Multi-controller xHCI and Intel real-hardware USB bring-up

- [x] Enumerate every PCI `0C:03:30` xHCI controller instead of stopping at the first match
- [x] Attempt xHCI initialization on each detected controller until one starts successfully
- [x] Increase the PCI device table capacity so real systems with many PCI functions do not hide USB controllers
- [x] Expose active xHCI PCI BDF, vendor/device IDs and BAR0 for hardware diagnostics
- [x] Report controller-specific xHCI initialization failures
- [x] Keep the generic xHCI driver usable for multiple Intel USB 3.x controllers
- [x] Keep USB 3.x root-port HID support on the active xHCI backend
- [ ] USB4 Host Router / Thunderbolt fabric driver
- [ ] Multi-controller HID aggregation across more than one active xHCI instance
