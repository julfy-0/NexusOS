# NexusOS storage drivers

| Device | Controller | Driver | Current scope |
|---|---|---|---|
| SATA | AHCI | `ahci.c` | polling, first ATA port, read-only |
| NVMe | PCI NVMe | `nvme.c` | admin queue, identify, one I/O queue, namespace 1, polling, read-only |

## NVMe notes

`nvme.c` targets the standard PCI NVMe programming interface. It discovers
PCI class `01:08:02`, maps BAR0, creates 4 KiB-aligned admin queues, enables the
controller, runs Identify Controller/Namespace, creates one I/O SQ/CQ pair and
implements `READ` commands.

The current block API uses 512-byte sectors. A namespace with another logical
block size is detected but rejected by `nvme_init()` until the block layer can
express native sector sizes.

DMA currently relies on NexusOS's identity-mapped physical/virtual address
model. The PRP path supports a 64 KiB read with a PRP list. Interrupts,
MSI/MSI-X, multiple namespaces/queues, write/flush commands, power management
and error recovery are intentionally not enabled yet.
