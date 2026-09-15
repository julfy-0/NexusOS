# NexusOS 0.5.18 — Enstein

## App Manager Foundation

NexusOS 0.5.13 retains the App Manager and Application Discovery foundations
and adds the first writable storage boundary needed for future installation.

- Built-in applications are registered through a dedicated helper.
- Applications have stable IDs, names, entry metadata and an explicit source:
  `builtin`, `system` or `userdata`.
- The manager tracks `stopped` / `running` state and launch counts.
- Only one application is considered active at a time.
- Stopping the active application returns the system to the desktop state.
- The lifecycle API does not pretend to execute binaries: ELF loading and
  userspace process execution remain future work.

The existing storage mount points `/system/apps` and `/userdata/apps` remain the
reserved locations for packaged applications. Discovery is still read-only at the
package-manager level, while the underlying FAT32/VFS backend can now create
directories and create/overwrite bounded 8.3 files.

## Application Discovery

NexusOS 0.5.13 retains the read-only application discovery path introduced in
0.5.11. The package manager scans `/system/apps` and
`/userdata/apps`, parses `manifest.nxm`, and registers valid applications with
the matching source classification.

Discovery itself does not install, extract, or execute applications. ELF loading
and userspace execution remain separate milestones. The new writable FAT32
primitive is intentionally lower-level and is not yet exposed as a package
installer.

## Networking

The 0.5.9 E1000 networking foundation remains integrated. ARP, IPv4, ICMP, UDP
and TCP are still future work.

## Writable FAT32 Foundation

- AHCI supports bounded `WRITE DMA EXT` sector commands.
- FAT32 can allocate/free clusters and update mirrored FAT copies.
- FAT32 can create directories and create/overwrite short 8.3 files.
- The active `/mnt/disk0` mount is read-write.
- VFS `mkdir`, `touch`, and `write` route to FAT32 when the resolved mount is writable.
- LFN, journaling, permissions, unlink/rename, transactions and executable loading remain future work.


## 0.5.13 — Package Installation Transaction

The Package Manager now provides a conservative transactional installation path. A package directory is validated before mutation, an application directory is staged, the optional single Entry payload is copied with a bounded 64 KiB limit, and `manifest.nxm` is written last as the publication point. Failures roll back the newly created payload and directory when possible.


## 0.5.14 — Multi-file package payload

The Package Manager now accepts an optional `Files` manifest field for up to eight root-level 8.3 payload files. Every payload is preflight-validated before mutation, copied into the staging directory, and removed in reverse order on rollback. `manifest.nxm` remains the final publication point.


## 0.5.15 — ELF64 user-space loader foundation

NexusOS can now validate and load small ELF64 x86_64 executables from the
mounted FAT32 filesystem. `elf-run <path>` creates a process, maps the ELF
load segments with user permissions, allocates a user stack and enters Ring 3
through the existing IRETQ transition.

This is intentionally a synchronous execution foundation: the current active
CR3 is still shared, syscall entry/return is not enabled, and scheduler-owned
user processes remain future work.


## Syscall entry/return

NexusOS 0.5.17 adds the first user-visible syscall entry path through `INT 0x80`. The IDT gate is DPL3, the entry stub saves all general-purpose registers, dispatches NOP/GETPID, returns the result in RAX, and restores the original CPL3 IRETQ frame. `EXIT` remains deferred until scheduler-owned user processes exist; the entry path deliberately refuses to return into an already-terminated process.


## Private CR3 / Process Address Spaces

NexusOS 0.5.18 gives each process a private copy of the kernel page-table hierarchy. User mappings are created only in that process CR3, and the scheduler switches CR3 together with the scheduler TCB. User image initialization no longer relies on the active kernel virtual address for the destination; it writes through the process page-table translation into the physical user pages.

The kernel identity map remains available in each address space so existing kernel code and hardware mappings continue to work. The higher-half kernel remains intentionally deferred.
