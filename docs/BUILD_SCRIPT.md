# NexusOS Build Script

`build.sh` is a frontend over the existing Makefile. It does not replace Make.

It uses `NEXUS_BUILD_JOBS` when provided and otherwise detects available CPU
cores. The displayed module progress is derived from real expected build
artifacts rather than timed/fake percentages.

Modules shown by the dashboard:

- Kernel
- Drivers
- Bootloader
- OS (overall project artifact progress)

Example:

```bash
NEXUS_BUILD_JOBS=8 ./build.sh
```
