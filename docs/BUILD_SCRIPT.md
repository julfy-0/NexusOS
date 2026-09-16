# NexusOS build.sh

`build.sh` is now the only project shell entrypoint.

It can compile NexusOS, optionally create the GPT/FAT32 `NexusOS.img`, optionally create `NexusOS.iso`, and optionally launch QEMU.

Interactive mode:

```sh
./build.sh
```

The script asks:

```text
Want to create NexusOS.img? [Y]:
Image size [1G] (MB/GB/TB, e.g. 2048M, 4G, 1T):
Want to create NexusOS.iso? [Y]:
Want to launch NexusOS? [N]:
```

Supported image size suffixes are `M`, `G`, and `T` (also accepted in lowercase). A bare number is interpreted as MiB.

Non-interactive examples:

```sh
./build.sh --size 4G --run
./build.sh --no-img --run
./build.sh --image-only --size 8G
```

The previous standalone shell entrypoints were removed so build, image creation, ISO creation and QEMU launching share one implementation.
