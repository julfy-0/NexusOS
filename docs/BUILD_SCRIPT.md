# BUILD_SCRIPT.md

`build.sh` — frontend над существующим GNU Make. Это не отдельная build system.
Текущая версия проекта остаётся 0.5.1.

Запуск:

```sh
./build.sh
```

Инкрементально:

```sh
./build.sh --no-clean
```

Количество jobs:

```sh
NEXUS_BUILD_JOBS=8 ./build.sh
```

## Progress model

Dashboard tracks actual generated artifacts:

```text
Kernel       [################........]  66%
Drivers      [########################] 100%
Bootloader   [########################] 100%
OS           [########################] 100%
```

- `Kernel` — kernel-side object/final outputs
- `Drivers` — driver objects
- `Bootloader` — UEFI loader objects + EFI output
- `OS` — overall ratio across the complete output set

The script does not invent time-based percentages. A target is counted when
its expected output file exists.

## Source of truth

The Makefile remains authoritative for dependency discovery and actual build
commands. The frontend only selects parallelism, starts the build and renders
status.
