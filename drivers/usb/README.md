# NexusOS USB host controller subsystem

NexusOS now has a controller layer covering the classic x86 USB host-controller families:

| USB generation | Host controller | PCI class / ProgIF | NexusOS driver |
|---|---|---|---|
| USB 1.0 / 1.1 | UHCI | `0C:03:00` | `uhci.c` |
| USB 1.0 / 1.1 | OHCI | `0C:03:10` | `ohci.c` |
| USB 2.0 | EHCI | `0C:03:20` | `ehci.c` |
| USB 3.x / up to USB 3.2 capable xHCI hardware | xHCI | `0C:03:30` | `xhci.c` |

## Current implementation level

The legacy controllers (UHCI/OHCI/EHCI) currently provide controller discovery, PCI
setup, controller reset/start, schedule/ring foundation and root-port detection.
The existing xHCI driver remains the most complete implementation and additionally
contains device addressing, descriptors and HID boot-keyboard support.

This is intentionally a staged USB stack: controller support is separated from the
future USB device-class layer (HID, Mass Storage, hubs, etc.).

## USB version mapping

USB version and host-controller interface are related but are not the same thing.
USB 1.x devices can be attached through UHCI/OHCI, USB 2.0 through EHCI, and USB 3.x
through xHCI. Modern xHCI controllers can also manage USB 2.0/1.x devices through
their supported protocol/root-port definitions.
