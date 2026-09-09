# NexusOS hardware model

NexusOS no longer targets a specific laptop or desktop model. The kernel is x86_64/UEFI based and detects the machine at runtime.

## Detection sources

- CPU vendor/model: CPUID
- System manufacturer/model: SMBIOS/DMI provided by UEFI
- PCI devices: PCI configuration mechanism #1
- Graphics: PCI class/vendor + UEFI GOP framebuffer
- USB: xHCI discovery
- Storage: AHCI discovery

A detected machine is not rejected because it differs from a previous test machine. Unsupported hardware is reported as unavailable instead.

## Network hardware

NexusOS detects PCI network controllers by class/vendor. Intel Wi-Fi devices such as AX201 are reported when exposed through PCI. Broadcom USB adapters require USB device enumeration on top of the xHCI controller; the current xHCI stage only detects the controller and connected ports, so it does not falsely claim a Broadcom USB driver is active.

## Storage hardware

NVMe controllers are now detected through PCI class code `01/08/02`. The existing AHCI path remains available for SATA controllers. A full NVMe block driver is a separate implementation step.

## Firmware

The UEFI loader detects ACPI 1.0/2.0 RSDP and passes its address/revision to the kernel. This is the basis for proper power, battery, thermal and device discovery on real PCs.
