# NexusOS Memory Management

The memory subsystem is built around the rewritten UEFI boot contract and
the relocatable, identity-mapped x86_64 kernel.

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

The boot identity map still uses 2 MiB pages. The rewritten loader places the
relocatable kernel and long-lived boot metadata below 4 GiB, and new page-table
pages are also kept below 4 GiB because there is no permanent physical-memory
direct map yet.

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
