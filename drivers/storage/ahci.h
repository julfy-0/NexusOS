#ifndef NEXUSOS_AHCI_H
#define NEXUSOS_AHCI_H

#include <stdint.h>

/* NexusOS: минимальный AHCI-драйвер.
 *
 * Осознанные упрощения (честно, чтобы не было сюрпризов):
 *  - только ОДИН порт (первый живой SATA-диск, не ATAPI/CD, не port
 *    multiplier) — многодисковые машины увидят только первый диск;
 *  - только polling, без прерываний (просто ждём в цикле завершения
 *    команды — на реальном железе может быть чуть медленнее, чем с IRQ,
 *    но для чтения файлов конфигурации/бинарников более чем достаточно);
 *  - polling, без прерываний;
 *  - запись ограничена WRITE DMA EXT и тем же безопасным максимумом
 *    128 секторов за команду; файловая система выше отвечает за
 *    целостность своих структур.
 */

/* Ищет AHCI-контроллер на PCI, инициализирует первый рабочий SATA-порт.
 * Возвращает 1 при успехе, 0 если контроллер/диск не найден. */
int ahci_init(void);

/* Читает count секторов по 512 байт начиная с lba в buf.
 * buf должен вмещать count*512 байт. Возвращает 1 при успехе. */
int ahci_read_sectors(uint64_t lba, uint32_t count, void *buf);

/* Writes count 512-byte sectors using WRITE DMA EXT. */
int ahci_write_sectors(uint64_t lba, uint32_t count, const void *buf);

/* Есть ли рабочий диск после ahci_init()? */
int ahci_is_ready(void);

#endif
