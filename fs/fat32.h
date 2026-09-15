#ifndef NEXUSOS_FAT32_H
#define NEXUSOS_FAT32_H

#include <stdint.h>

/* NexusOS: FAT32 read/write foundation.
 *
 * Осознанные упрощения:
 *  - без длинных имён (LFN) — показываются короткие 8.3-имена, как их
 *    сгенерировала утилита форматирования (например "KERNEL~1.ELF");
 *  - без MBR/GPT — раздел монтируется с указанного LBA напрямую (подходит
 *    и для "сырых" superfloppy-образов вроде тех, что делает mkfs.fat без
 *    таблицы разделов, и для случая, когда LBA раздела ESP уже известен);
 *  - предполагается размер сектора 512 байт (стандарт для дисков, но не
 *    гарантирован спецификацией FAT).
 *
 * Запись поддерживается только через ограниченный AHCI write path;
 * see docs/FAT32_WRITE.md for the intentionally conservative limits.
 */

/* Монтирует FAT32-раздел, начинающийся с сектора partition_lba.
 * Возвращает 1 при успехе (нашли и распознали валидный VBR FAT32). */
int fat32_mount(uint64_t partition_lba);

int fat32_is_mounted(void);

/* Returns 1 when path exists and is a directory. */
int fat32_is_directory(const char *path);

/* Печатает содержимое директории path ("/" — корень) через console_print:
 * имя, [DIR] или размер в байтах. Возвращает 1 при успехе, 0 если путь
 * не найден или это не директория. */
int fat32_list(const char *path);

/* Enumerates short 8.3 directory entries without printing. Returns the number
 * copied into caller-owned arrays, or -1 on invalid/unmounted input. */
int fat32_list_entries(const char *path, char names[][13], unsigned char is_dir[], int max_entries);

/* Читает файл path целиком (до buf_size байт) в buf.
 * *out_size — сколько реально байт записано (может быть меньше размера
 * файла, если он больше buf_size). Возвращает 1 при успехе. */
int fat32_read_file(const char *path, void *buf, uint32_t buf_size, uint32_t *out_size);

/* Creates/overwrites an 8.3 file using the mounted FAT32 volume.
 * The operation allocates clusters as needed and updates both FAT copies.
 * Returns 1 on success, 0 on validation/device/filesystem failure. */
int fat32_write_file(const char *path, const void *buf, uint32_t size);

/* Deletes an 8.3 regular file and releases its cluster chain. */
int fat32_remove_file(const char *path);

/* Deletes an empty 8.3 directory (only . and .. may remain). */
int fat32_remove_empty_dir(const char *path);

/* Creates one 8.3 directory below an existing FAT32 directory. */
int fat32_mkdir(const char *path);

#endif
