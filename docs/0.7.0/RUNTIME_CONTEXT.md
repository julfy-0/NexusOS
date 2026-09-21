# NexusOS 0.7 — Native Runtime Context

The first 0.7 runtime layer exposed a low-level `nexus_runtime_info()` syscall wrapper. This increment adds a reusable userspace runtime context on top of that ABI without changing the syscall numbers or runtime ABI version.

## API

`nexus_runtime_context_open()` obtains a fresh runtime snapshot.

`nexus_runtime_context_refresh()` refreshes an existing context and marks it invalid when the runtime query fails.

The context provides stable accessors for:

- process PID and parent PID;
- private address-space identifier;
- runtime and syscall ABI versions;
- runtime feature bits;
- channel/message limits;
- runtime name.

`nexus_runtime_has_feature()` performs an all-bits feature test, so callers can safely require more than one capability bit at once.

## ABI compatibility

No existing syscall number was changed. No existing runtime ABI version was incremented. This is a userspace library layer built on the already-published 0.7 runtime contract.
