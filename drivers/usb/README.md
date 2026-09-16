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


## Real-hardware xHCI bring-up (0.5.29)

The xHCI driver now enumerates all PCI `0C:03:30` controllers and attempts
initialization on each controller instead of stopping at the first match. This
supports machines that expose two independent Intel USB 3.x/xHCI controllers.
The active controller identity (PCI BDF, vendor/device ID and BAR0) is exposed
for kernel diagnostics.

USB4 Host Router devices are intentionally not treated as xHCI controllers: the
existing xHCI driver handles standard USB 2/3 root ports, while USB4 fabric and
Thunderbolt tunneling remain a separate future driver layer.
