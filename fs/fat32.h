#ifndef NEXUSOS_FAT32_H
#define NEXUSOS_FAT32_H

#include <stdint.h>

/* NexusOS: FAT32, ТОЛЬКО ЧТЕНИЕ.
 *
 * Осознанные упрощения:
 *  - без длинных имён (LFN) — показываются короткие 8.3-имена, как их
 *    сгенерировала утилита форматирования (например "KERNEL~1.ELF");
 *  - GPT/MBR не разбирает сам драйвер: вызывающий слой передаёт начальный LBA;
 *    это позволяет отделить обнаружение разделов от FAT32.
 *  - до четырёх независимых FAT32 mount contexts поддерживаются одновременно;
 *  - предполагается размер сектора 512 байт (стандарт для дисков, но не
 *    гарантирован спецификацией FAT).
 *
 * Записи нет — см. ahci.h почему.
 */

/* Монтирует FAT32-раздел, начинающийся с сектора partition_lba.
 * Возвращает 1 при успехе (нашли и распознали валидный VBR FAT32). */
int fat32_mount(uint64_t partition_lba);
int fat32_mount_partition(uint64_t partition_lba, int slot);
int fat32_select_mount(int slot);

int fat32_is_mounted(void);

/* Returns 1 when path exists and is a directory. */
int fat32_is_directory(const char *path);

/* Печатает содержимое директории path ("/" — корень) через console_print:
 * имя, [DIR] или размер в байтах. Возвращает 1 при успехе, 0 если путь
 * не найден или это не директория. */
int fat32_list(const char *path);

/* Читает файл path целиком (до buf_size байт) в buf.
 * *out_size — сколько реально байт записано (может быть меньше размера
 * файла, если он больше buf_size). Возвращает 1 при успехе. */
int fat32_read_file(const char *path, void *buf, uint32_t buf_size, uint32_t *out_size);

#endif
