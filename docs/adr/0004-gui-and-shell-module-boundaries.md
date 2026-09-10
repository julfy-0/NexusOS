# ADR 0004 — GUI and shell module boundaries

- Status: Accepted
- Date: 2026-09-10
- Scope: source-tree organization only

## Context

NexusOS 0.5.1 already had working GUI applications and a working shell, but
GUI rendering, application behavior and input handling were concentrated in
one GUI source file, while shell command routing lived in a long conditional
chain inside the shell input module.

The project needs a structure that is easier to extend in preparation for
future 0.5.2 work without changing the monolithic kernel model or rewriting
working functionality.

## Decision

Split the existing GUI into these boundaries:

- `gui/core/` — lifecycle and shared GUI state
- `gui/renderer/` — drawing primitives, text and font boundary
- `gui/desktop/` — desktop surface/dock/menu
- `gui/input/` — keyboard/mouse GUI event handling
- `gui/search/` — search UI and actions
- `gui/apps/` — Files, Terminal and Settings

Keep the public `gui_*` API in `gui/core/gui.h` stable.

Split shell dispatch into `shell/core/command_registry.c`, while retaining
all command implementations in their existing functional categories.

Move the FAT32 implementation from `fs/fat32.c` to `fs/fat32/fat32.c` so the
filesystem tree represents the VFS/concrete-FS boundary explicitly.

## Consequences

Positive:

- New GUI applications do not require editing the GUI renderer/core file.
- Input policy is separated from framebuffer rendering.
- Shell command registration is data-driven and centralized.
- Object paths continue to mirror source paths, so filename collisions remain
  unlikely.
- Existing runtime behavior and public entry points are preserved.

Trade-offs:

- There are more small source files.
- GUI modules share one kernel-side context because NexusOS is still monolithic.
- The command registry still links all commands into the kernel.

## Not decided here

This ADR does not introduce a compositor, heap, scheduler, processes, ring 3,
new ABI, new filesystem semantics, or a new build system.
