#ifndef NEXUSOS_NVME_H
#define NEXUSOS_NVME_H

#include <stdint.h>

/* NexusOS NVMe driver.
 *
 * Current scope:
 *  - PCI NVMe 1.x/2.x-compatible controllers (PCI class 01:08:02)
 *  - one controller, namespace 1
 *  - admin queue + one I/O submission/completion queue
 *  - Identify Controller / Identify Namespace
 *  - polling, no MSI/MSI-X/interrupt dependency
 *  - 512-byte logical sectors
 *  - read path using PRP1 + PRP list (up to 128 sectors / 64 KiB)
 *
 * The driver intentionally starts read-only. Filesystem write support should
 * be added only after the block layer and crash/error handling are ready.
 */

int nvme_init(void);
int nvme_is_ready(void);
int nvme_read_sectors(uint64_t lba, uint32_t count, void *buf);

uint32_t nvme_sector_size(void);
uint64_t nvme_namespace_sectors(void);
uint16_t nvme_controller_vendor_id(void);
uint16_t nvme_controller_device_id(void);
uint8_t nvme_version_major(void);
uint8_t nvme_version_minor(void);

#endif
