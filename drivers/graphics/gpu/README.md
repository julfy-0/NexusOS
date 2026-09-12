# NexusOS GPU Detection Drivers

This layer provides real PCI display-controller detection and vendor/device name resolution.

## Supported detection

The manager scans PCI class `0x03` display controllers and records:

- PCI BDF (`bus:device.function`)
- vendor ID / device ID
- PCI class/subclass/programming interface
- BAR0 address
- vendor name
- human-readable GPU model when the device ID is in the built-in database

Vendor databases currently cover Intel, AMD, NVIDIA, QEMU, VirtIO and VMware adapters.
Unknown models are never guessed: NexusOS reports the vendor and falls back to a generic model name.

## Important

This is a **GPU identification and initialization foundation**, not yet a full accelerated 2D/3D driver. The UEFI GOP framebuffer remains the active display backend. Vendor register programming, VRAM management, command submission, modesetting and hardware acceleration are separate future layers.
