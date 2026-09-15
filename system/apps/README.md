# NexusOS System Applications

The App Manager is the central registry for applications exposed by NexusOS.
Built-in applications are registered during kernel initialization and may be
looked up by stable application ID.

Current built-ins:

- `nexus.files` — Files
- `nexus.terminal` — Terminal
- `nexus.settings` — Settings

The manager now tracks application source (`builtin`, `system`, `userdata`),
active/stopped state and launch counts. It deliberately does **not** execute
ELF binaries yet; user-space execution remains a later milestone.

`/system/apps` and `/userdata/apps` remain reserved filesystem locations for
future packaged applications. Persistent directory enumeration and writable
installation are still pending.
