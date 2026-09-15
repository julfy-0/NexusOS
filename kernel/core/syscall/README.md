# NexusOS syscall foundation

NexusOS 0.5.4 provides a small kernel-side dispatch ABI for future Ring-3 code.
Supported dispatch numbers are NOP, GETPID and EXIT. Hardware SYSCALL/SYSRET
entry is intentionally not enabled yet: a safe user return path, process CR3
switching and complete exception handling must exist first.

This keeps 0.5.4 honest: the process/userspace interfaces are integrated and
buildable, but NexusOS does not yet claim runnable isolated user programs.
