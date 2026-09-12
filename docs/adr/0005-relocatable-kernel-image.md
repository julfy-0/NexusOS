# ADR-0005 — Relocatable Kernel Image

## Status

Accepted for the 0.5.3 boot/kernel rewrite.

## Decision

NexusOS builds the freestanding x86_64 kernel as an `ET_DYN` ELF image using
`-fPIE`. The UEFI loader allocates one contiguous physical image below 4 GiB,
loads all PT_LOAD segments, and applies `R_X86_64_RELATIVE` relocations before
calling `ExitBootServices()`.

## Why

The old loader required every PT_LOAD segment to be allocated at its exact
link-time physical address (`0x200000`). That made boot dependent on whether
UEFI/OVMF happened to leave that exact region available and caused failures
such as `AllocatePages for segment failed`.

The new contract separates:

- **link-time image address** — `0x200000`
- **runtime physical image address** — selected by UEFI below 4 GiB
- **runtime entry** — calculated from the load bias

## Constraints

- Only `R_X86_64_RELATIVE` relocations are accepted.
- The current page-table implementation requires the runtime kernel image to
  remain below 4 GiB.
- The kernel is still identity-mapped; this ADR does not introduce a
  higher-half address space.
- No dynamic linker is present after kernel hand-off.
