# NexusOS build.sh

`build.sh` is the pretty command-line build frontend for NexusOS 0.5.0 - Enstein.

Run from the project root:

```sh
./build.sh
```

For an incremental build without removing `build/` and `iso/` first:

```sh
./build.sh --no-clean
```

The four progress lines represent real build stages:

```text
Kernel       [################........]  66%
Drivers      [########################] 100%
Bootloader   [########################] 100%
OS           [########################] 100%
```

The progress bar advances after successful Make targets. It is a frontend over the existing Makefile, not a separate build system.


### Parallel build

`build.sh` runs GNU Make with multiple jobs (`NEXUS_BUILD_JOBS` can override the worker count). The four progress bars update together while compilation is happening. `OS` is the overall percentage of the complete build artifact set, not a separate fake stage.
