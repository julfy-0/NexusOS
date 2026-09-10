# ARCHITECTURE.md

## Type of kernel

NexusOS is a **monolithic kernel** (`docs/adr/0001`). The project was moved
to UEFI/x86_64 by `docs/adr/0002`; the current work does not change either
decision.

The source tree is modular, but source directories are not separate kernel
processes. All linked kernel-side modules still execute in ring 0.

## Source-tree boundaries

```text
boot/uefi/          UEFI loader
kernel/core/        kernel orchestration, state, panic, usermode foundation
kernel/arch/x86_64/ x86_64 entry/GDT/IDT/ISR/linker
kernel/mm/          paging

drivers/            hardware/input/storage/graphics/timer/USB
fs/vfs/             VFS core, mounts, filesystem registry
fs/fat32/           FAT32 implementation

gui/core/           GUI lifecycle and shared state
gui/desktop/        desktop/dock/menu surface
gui/apps/           Files, Terminal, Settings
 gui/input/         GUI keyboard/mouse event handling
gui/renderer/       framebuffer primitives, text and font boundary
gui/search/         desktop search

shell/core/         shell line editor + command registry
shell/commands/     command implementations grouped by function
lib/                freestanding reusable code
assets/             static, replaceable assets
include/nexus/      shared public headers
platform/target/    runtime hardware inventory
```

The GUI and shell boundaries are deliberately above their hardware and
storage dependencies: PS/2 and framebuffer drivers do not contain GUI policy,
and individual shell commands do not implement command-line parsing or
registration.

## Boot flow

```text
UEFI Firmware
  → boot/uefi/src/boot.c: efi_main(ImageHandle, SystemTable)
      1. GOP framebuffer
      2. read kernel.elf from the boot volume
      3. parse ELF64 and place PT_LOAD segments
      4. final GetMemoryMap() + ExitBootServices()
      5. jump to kernel entry with nexus_boot_info_t* in RDI
  → kernel/arch/x86_64/entry.S: _start
      own stack → kmain(rdi)
  → kernel/core/kernel.c: kmain()
      console → GDT → IDT → paging → PIC/PIT → PS/2 input
      → PCI/AHCI/FAT32 → xHCI → platform detection → shell
```

The UEFI loader and kernel use different ABIs. The loader is compiled for
the UEFI/MS x64 environment; the kernel uses the System V x86_64 ABI. The
ABI boundary is the handoff into the kernel entry point and is not changed by
the directory reorganization.

## Interrupts

The IDT contains vectors 0–31 for CPU exceptions and 32–47 for PIC IRQs.
`kernel/arch/x86_64/isr.S` preserves the interrupt frame and calls
`isr_handler()` in `kernel/arch/x86_64/idt.c`.

Current hardware dispatch remains intentionally simple: IRQ0 drives the PIT,
IRQ1 the keyboard and IRQ12 the PS/2 mouse. The PS/2 drivers remain hardware
modules; GUI policy is handled later by `gui/input/`.

## Memory

`kernel/mm/paging.c` owns the current page-table implementation. It builds
PML4/PDPT/PD tables using 2 MiB pages, maps the required identity regions,
and switches CR3. Page faults are diagnosed by the IDT exception path.

The current architecture intentionally does **not** introduce a heap or
higher-half kernel as part of this restructuring. Those remain future memory
milestones.

## GUI architecture

The GUI was previously concentrated in one `gui/core/gui.c`. It is now split
without changing the external `gui_*` API:

```text
PS/2 keyboard ─┐
               ├→ gui/input/ ─────┐
PS/2 mouse ─────┘                  │
                                   ↓
                             gui/core/gui.c
                                   │
                 ┌─────────────────┼──────────────────┐
                 ↓                 ↓                  ↓
             desktop/           search/           apps/
                 │                                   ├→ files/
                 └───────────────┬───────────────────┼→ terminal/
                                 ↓                   └→ settings/
                           renderer/
```

`gui/core/gui_state.*` owns the single GUI context. It prevents each GUI
module from maintaining its own copy of framebuffer/view state.
`gui/renderer/` owns drawing primitives and the current bitmap-font boundary.
Application modules own application-specific rendering and actions.

This is preparation for a future window system, not a compositor rewrite.

### Window-system preparation

A future window manager can be added behind the existing GUI lifecycle. The
current split provides natural places for window state, focus and hit testing
without forcing a new compositor or changing working applications now.

## Shell architecture

The command implementations remain in:

```text
shell/commands/system/
shell/commands/filesystem/
shell/commands/utilities/
```

`shell/core/shell.c` owns the input line, history and argument splitting.
`shell/core/command_registry.c` owns command registration, dispatch, help
metadata and the small set of shell-integrated commands (`mount`,
`desktop-run`, `clear`, etc.).

This removes command routing from the shell input editor while preserving all
existing command names and implementations.

## Filesystem architecture

```text
shell commands / GUI Files
          ↓
       fs/vfs/core
          ↓
    fs/vfs/mount + registry
          ↓
      concrete FS
          ↓
       fs/fat32
          ↓
       drivers/storage
```

The RAM-backed VFS remains independent of the concrete FAT32 implementation.
The FAT32 source is kept under `fs/fat32/` so the filesystem boundary is
visible in the source tree.

## Include/dependency policy

- Shared interfaces belong in `include/nexus/` when they cross major
  subsystems (for example `boot_info.h`).
- Module-private headers stay beside their module.
- Source files should include the nearest module header rather than reaching
  through long relative paths.
- GUI application code may depend on the GUI renderer/state and VFS APIs, but
  the framebuffer driver must not depend on individual GUI applications.
- Shell command implementations may depend on kernel services, but shell
  input parsing/registration belongs in `shell/core/`.
- Keep the kernel freestanding: no libc requirement and no C++.
