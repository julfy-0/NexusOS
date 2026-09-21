# NexusOS 0.7 — Native Userspace Foundation

The 0.7 line begins the Native Nexus Architecture milestone.

## 0.7 implemented foundation

- Public, append-only syscall ABI header.
- Nexus Runtime ABI and feature descriptor.
- Built-in `runtime` service module.
- Built-in `ipc` service module.
- Fixed-size Nexus Channel IPC.
- Per-process channel handles.
- Bounded 256-byte messages with eight queued messages per endpoint.
- Cross-process channel creation, send, receive, poll and close.
- Process-exit channel cleanup.
- Userspace runtime wrapper library using `INT 0x80`.
- Runtime inspection through a new syscall.

## Design

NexusOS does not model the new userspace layer around Linux userland concepts.
The native model is based on runtime services, process-owned handles and Nexus
channels. Existing UEFI, PCI, USB, ELF and FAT32 standards remain hardware/file
format interoperability layers only.
