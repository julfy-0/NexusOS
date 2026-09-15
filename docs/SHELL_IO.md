# NexusOS Shell I/O

NexusOS 0.5.5 keeps the existing kernel-resident command registry and adds a
small bounded I/O transport without rewriting every command.

## Supported syntax

- `cmd1 ; cmd2`
- `cmd1 && cmd2`
- `cmd1 | cmd2`
- `cmd < file`
- `cmd > file`
- `cmd >> file`

Quotes and backslash escapes are handled by `shell/core/parser.c`.

## Execution model

Existing commands still print through `console_print()` and `console_putchar()`.
For a pipeline or output redirection, the console temporarily captures that
output into a bounded shell buffer. The shell then either passes the captured
text to the next command as its input payload or writes/appends it to the
current VFS backend.

Input redirection reads from the current VFS backend and provides the text as
an input payload to the command adapter. This is a foundation layer, not a
POSIX-compatible `stdin` implementation: commands retain their current
`char *args` API and are not yet backed by per-process file descriptors.

## Limits

- I/O capture buffer: 2048 bytes.
- File redirection currently targets the writable in-memory VFS backend.
- Mounted FAT32 remains read-only until persistent FAT32 write support is
  implemented.
- File descriptors and user-space processes are future work.

The design intentionally reuses the existing command registry and console
instead of introducing a second shell or duplicating command implementations.
