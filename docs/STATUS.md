# NexusOS 0.6.0 — Enstein

NexusOS 0.6.0 is the start of the large integration milestone. The source currently includes the 0.5.x userspace/process/storage foundations plus a new font service, compact colored startup logging, and hardware/process/module inspection commands.

The full 0.6.0 target is tracked in `docs/0.6.0_PLAN.md`: 128+ feature targets and 200 hardening/fix targets. Those targets remain open until each item is implemented and validated.

## Implemented in this 0.6.0 foundation build

- Version and build pipeline bumped to 0.6.0.
- Startup result format is exactly `[  OK  ]` or `[ FAIL ]`, with green/red status.
- Module/service names are displayed with their type and version.
- Roboto Regular and Bold TTF assets are embedded into `kernel.elf`.
- Roboto font service reports asset sizes and readiness.
- Added `modules` command.
- Added `services` command.
- Added `fontinfo` command.
- Added `ps` command.
- Added `free` command.
- Added `hostname` command.
- Added `arch` command.
- Added `.gitattributes` LF enforcement.
- `/old` remains ignored.

## Runtime validation limitation

The local build environment does not provide QEMU or the user's physical PC, so USB and graphical runtime behavior cannot be confirmed here.
