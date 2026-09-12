# NexusOS Shell

The NexusOS shell is split into two layers:

- `shell/core/` — input buffering, command history and command dispatch.
- `shell/commands/` — individual command implementations grouped by purpose.

## Command registry

`shell/core/command_registry.c` is the single source of truth for commands.
Every command entry contains:

- canonical command name;
- optional alias;
- category;
- usage syntax;
- short description;
- implementation callback.

This prevents the shell dispatcher and `help` output from drifting apart.

## Help

```text
help
help system
help hardware
help filesystem
help text
help math
help shell
help power
help <command>
man <command>
```

Aliases currently include:

- `dir` -> `ls`
- `cls` -> `clear`
- `?` -> `help`

## Adding a command

1. Put the implementation in the appropriate `shell/commands/<category>/` directory.
2. Add its header.
3. Add a small `CMD0` or `CMD1` wrapper in `command_registry.c`.
4. Add one `ENTRY(...)` record to the registry.
5. `help`, `man` and dispatch will automatically use the same metadata.

Do not add a second `strcmp()` dispatch chain to `shell.c`.
