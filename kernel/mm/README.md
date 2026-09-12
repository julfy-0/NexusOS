# NexusOS Memory Management

The memory subsystem is built incrementally around the current UEFI boot
contract and higher-half x86_64 kernel with a temporary low identity map retained for early boot.

## PMM

`pmm.c/.h` owns physical 4 KiB pages using a bitmap built from the UEFI memory
map. Only `EfiConventionalMemory` is released to the allocator; the kernel,
boot information, memory map, framebuffer and low memory remain reserved.

## VMM

`vmm.c/.h` provides 4 KiB page mapping on top of the existing page tables:

- map/unmap a virtual page
- translate virtual to physical
- query mappings
- invalidate the affected TLB entry

The boot identity map still uses 2 MiB pages. The kernel is linked at
`0xFFFFFFFF80000000` but is physically loaded beginning at `0x00200000`. A
small assembly trampoline creates a temporary identity + higher-half alias,
then transfers execution to the high virtual address. The kernel subsequently
builds its own PML4 and keeps the low identity map during this stage. New
page-table pages are still required to live below 4 GiB because there is no
permanent physical-memory direct map yet.

## Kernel heap

`heap.c/.h` provides the first general-purpose kernel allocator:

- `kmalloc(size)` — 16-byte aligned first-fit allocation
- `kfree(ptr)` — free and coalesce adjacent blocks
- dedicated high virtual arena at `0xFFFF800000000000`
- PMM-backed pages mapped through the VMM on demand
- free blocks remain mapped and are reused
- allocator metadata lives inside the heap itself
- allocation/free operations temporarily disable interrupts to prevent
  re-entrant list corruption in the current pre-scheduler kernel

This is intentionally a kernel-only heap. It is **not** yet a user-space
allocator and it does not provide process address spaces, guard pages,
slab caches, or per-CPU heaps. Those belong to later memory/process milestones.

## Page-fault diagnostics (0.5.2.4)

`page_fault.c` handles x86_64 exception vector 14 separately from the generic
exception dispatcher. It decodes `CR2` and the CPU page-fault error code,
prints the access type, privilege level, protection state, `RIP`, `CS`,
`RFLAGS`, `CR3`, and user `RSP/SS` when applicable, then enters the existing
panic/restart path.

The handler intentionally does **not** walk the VMM page tables or attempt
recovery. A damaged page-table hierarchy could make a diagnostic page-table
walk recursively fault. Actual demand paging and process-level recovery are
future milestones.


## Higher-half kernel (0.5.2.5)

The linker separates the kernel virtual address (VMA) from its physical load
address (LMA):

- physical load base: `0x00200000`
- kernel virtual base: `0xFFFFFFFF80000000`
- `_start`: low-physical bootstrap entry
- `kernel_high_entry`: higher-half execution entry

`kernel/arch/x86_64/entry.S` builds bootstrap page tables before the C kernel
can run at its linked address. The bootstrap maps the first 4 GiB identity and
aliases that range at the higher-half kernel window. `paging_init()` then
rebuilds the final page tables and installs the permanent higher-half kernel
alias while retaining the low identity mapping for boot information, MMIO and
physical page-table access.

The PMM now reserves the physical kernel image using physical linker symbols,
not the higher-half virtual addresses.
