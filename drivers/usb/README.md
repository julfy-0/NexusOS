# NexusOS USB subsystem

## Stage 1 — xHCI controller

Implemented now:

- PCI discovery of USB xHCI controllers (`0C:03:30`)
- 32-bit and 64-bit PCI BAR support
- PCI Memory Space and Bus Master enable
- xHCI Legacy Support ownership handoff
- Controller halt and reset
- Port count and connected-port detection
- QEMU test configuration with `qemu-xhci` and a USB keyboard

## Next stages

1. DCBAA allocation
2. Command Ring
3. Event Ring
4. Enable Slot command
5. Address Device command
6. Read device descriptor
7. Configuration/interface parsing
8. USB HID keyboard and mouse
9. USB Mass Storage (BOT)

The current stage is deliberately a controller foundation, not yet a full USB device stack.
