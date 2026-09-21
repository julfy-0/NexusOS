# NexusOS 0.7 — Enstein

NexusOS 0.7 is the start of the large integration milestone. The source currently includes the 0.5.x userspace/process/storage foundations plus a new font service, compact colored startup logging, and hardware/process/module inspection commands.

The large 0.7 program is split into 15 patches. 0.7 starts the native runtime foundation; later 0.7 patches build IPC, capabilities, drivers, desktop, applications and networking on top of it.

## Current 0.7 implementation block — Service/IPC dependency manager

- Module descriptors now carry explicit dependency lists.
- Service initialization recursively loads required dependencies before the dependent service.
- Missing dependencies, self-dependencies and dependency cycles fail the dependent module cleanly.
- Runtime service explicitly depends on the Nexus IPC service instead of relying only on priority ordering.
- Added dependency inspection helpers for service diagnostics.

## Implemented in this 0.7 foundation build

- Version and build pipeline bumped to 0.7.
- Startup result format is exactly `[  OK  ]` or `[ FAIL ]`, with green/red status.
- Built-in Roboto Regular and Bold TTF assets remain embedded into `kernel.elf`.
- Public, append-only syscall ABI moved to `include/nexus/syscall_abi.h`.
- Nexus Runtime ABI added in `include/nexus/runtime_abi.h`.
- Added built-in `ipc` and `runtime` service modules.
- Added fixed-size Nexus Channel IPC with per-process channel handles.
- Added runtime information and channel create/close/send/receive/poll syscalls.
- Added userspace runtime wrapper library under `user/runtime/`.
- Process termination now closes owned IPC endpoints before entering the zombie lifecycle.
- Existing 0.6.x stability, watchdog, heap validation, private CR3 and Critical OS Stop paths are preserved.

## Runtime validation limitation

The local build environment does not provide QEMU or the user's physical PC, so USB and graphical runtime behavior cannot be confirmed here.


### Shell stability hotfix
- Shell command execution is deferred through a bounded 4-entry queue.
- Keyboard and xHCI input paths only buffer commands; command execution runs in the normal kernel loop after event draining.


## NexusOS 0.7 — Build 002

Native capability foundation is integrated. Processes now carry a native capability bitmap inherited from their parent. Sensitive syscall families are capability-gated, and userspace can inspect or voluntarily drop capabilities through the append-only syscall ABI. Capability granting remains kernel-owned.
