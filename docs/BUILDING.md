# Building NexusOS 0.5.2

## Requirements

Kernel/bootloader build:

- x86_64 Linux
- GCC
- GNU binutils
- GNU Make
- Python 3 for the dependency-free image builder

QEMU runtime additionally requires:

- `qemu-system-x86_64`
- OVMF firmware

The project does not require CMake, a C++ compiler, or a kernel libc.

## Build

```bash
make clean
make
make iso
```

The compatibility `iso/` staging area contains the EFI bootloader and a legacy
copy of `kernel.elf`. The production disk image is created separately by
`create-img.sh`.

The parallel frontend can be used with:

```bash
NEXUS_BUILD_JOBS=8 ./build.sh
```

## Disk image

Interactive:

```bash
./create-img.sh
```

Non-interactive:

```bash
./create-img.sh --userdata 512M
./create-img.sh --userdata 1G
./create-img.sh --userdata 2G
./create-img.sh --userdata 4G
./create-img.sh --userdata 10G
```

The resulting `NexusOS.img` is a real GPT image with three real FAT32
partitions. `tools/create-fat32.py` is used when host `dosfstools`/`mtools`
are unavailable, so image creation does not silently fall back to a single
folder-backed image.

## Run

```bash
./run.sh
```

or:

```bash
./run.sh --window
./run.sh --no-kvm
./run.sh --mem 512
./run.sh --rebuild
```

Set the default USERDATA size for automatic image creation with:

```bash
NEXUS_USERDATA_SIZE=2G ./run.sh
```
