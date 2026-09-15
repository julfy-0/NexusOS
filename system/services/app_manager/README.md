# NexusOS App Manager

The App Manager is the runtime registry and lifecycle-state boundary for
NexusOS applications.

## Current API

- `nexus_app_register()` — register one application descriptor.
- `nexus_app_register_builtin()` — register a kernel-integrated built-in app.
- `nexus_app_find()` / `nexus_app_at()` — enumerate or resolve applications.
- `nexus_app_runtime()` — inspect lifecycle state and launch count.
- `nexus_app_launch()` — mark an application active and enter application state.
- `nexus_app_stop()` / `nexus_app_stop_id()` — stop an application and return to desktop when appropriate.

The lifecycle layer is intentionally separate from binary execution. An
application entry such as `builtin:files` is metadata at this stage; it is not
an ELF loader or a userspace process launch mechanism.

## Limits

The registry is fixed at 16 applications. Read-only `/system/apps` and `/userdata/apps` manifest discovery is now
integrated through the Package Manager. Writable installation, ELF loading and
full userspace execution remain future work.


## User ELF execution

The application registry remains the lifecycle layer. User ELF loading is
provided by `kernel/core/elf_loader.*` and is currently exposed through the
`elf-run <path>` shell command. Full App Manager-to-process execution waits for
private address spaces, syscall entry/return and scheduler-owned user tasks.
