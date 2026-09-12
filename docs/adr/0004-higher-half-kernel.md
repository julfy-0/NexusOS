# ADR-0004: Higher-half kernel at 0xFFFFFFFF80000000

## Status

Accepted — NexusOS 0.5.2.5.

## Context

NexusOS previously linked and executed the kernel at its physical load address
(`0x00200000`). That made kernel pointers equal to physical addresses and tied
normal execution to the identity mapping.

A real user/kernel split requires a stable kernel virtual address space that is
independent from physical placement. The transition must also happen before
normal C code executes at its new linked addresses.

## Decision

Link the normal kernel at:

```text
Virtual:  0xFFFFFFFF80000000
Physical: 0x0000000000200000
```

Keep a tiny `.text.entry` trampoline at the physical entry address. It builds
temporary page tables which provide both:

- low identity mapping for the first 4 GiB;
- higher-half alias beginning at `0xFFFFFFFF80000000` for the same physical
  range.

The trampoline then jumps to `kernel_high_entry`. From that point onward,
normal C code executes at the higher-half virtual address.

`paging_init()` subsequently rebuilds NexusOS-owned page tables and installs
the permanent higher-half kernel alias while retaining the low identity map.
The identity map remains temporarily useful for boot information, physical
MMIO and direct access to low page-table pages until a dedicated physical
memory direct map is introduced.

## Consequences

### Positive

- Kernel virtual addresses no longer depend on physical placement.
- The heap can live in a separate high virtual arena.
- A future user-space address range can be placed below the kernel without
  colliding with kernel code/data.
- PMM can correctly reserve the physical kernel using physical linker symbols.

### Trade-offs

- The UEFI ELF loader still loads PT_LOAD segments by `p_paddr`.
- Bootstrap assembly is required before the higher-half C entry.
- The low identity map is intentionally retained for now; removing it belongs
  to the future direct-map/address-space work.
- Page-table pages allocated dynamically are still required to be below 4 GiB
  because NexusOS does not yet have a permanent physical-memory direct map.
