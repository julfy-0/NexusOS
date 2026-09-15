# NexusOS Process Foundation

## NexusOS 0.5.5 — Process Foundation

This subsystem introduces the first explicit kernel process object without
pretending that NexusOS already has a complete userspace.

Each process has a stable PID, parent PID, lifecycle state, private CR3 ownership,
and metadata for a reserved user virtual range. The table is fixed at eight
entries for now. Process metadata is protected by an IRQ-safe spinlock because
future scheduler and syscall paths can share this state with interrupt-adjacent
kernel execution.

Implemented API:

- `process_init()`
- `process_create()`
- `process_get()`
- `process_set_current()` / `process_current()`
- `process_exit()` / `process_exit_with_code()`
- `process_reap()`
- `process_zombie_count()`
- `process_reserve_user_range()`

Not implemented in this block:

- private page tables / CR3 switching (0.5.18)
- physical user-page allocation and mapping
- Ring 3 transition metadata and IRETQ entry ABI
- syscall ABI
- ELF loading

Those are deliberately separate follow-up blocks of milestone 0.5.4.


### 0.5.17
Processes may be owned by a scheduler TCB through `scheduler_thread_id`. User process termination clears the process state before the scheduler reclaims the kernel thread stack.


### Lifecycle (0.5.20)

Processes now use a two-phase termination lifecycle. `process_exit()` marks a
process as `PROCESS_ZOMBIE` and records its exit metadata, but does not destroy
the currently active CR3 or user pages. The scheduler-owned TCB is then marked
zombie by `thread_exit()`. Once a different TCB is running, the scheduler calls
`process_reap()` to release user mappings, destroy the private address space and
return the process slot to `PROCESS_UNUSED`. PIDs are therefore not reused while
a process is still a zombie.
