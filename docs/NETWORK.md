# NexusOS networking

NexusOS 0.5.9 contains the first real NIC binding for Intel E1000-compatible PCI
controllers (including the common QEMU 8086:100E device). The driver enables PCI
memory/bus-master access, validates MMIO register access and reads the hardware MAC
address. Packet DMA is deliberately not enabled yet: RX/TX descriptor rings, ARP,
ICMP, UDP and TCP still require their own tested implementation.

Architecture remains deferred: hardware drivers do not execute shell or GUI work in
IRQ context.
