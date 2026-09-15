# NexusOS FAT32 Write Foundation

NexusOS 0.5.12 adds a deliberately bounded write path to the existing FAT32
backend. It remains a simple filesystem implementation, not a journaling or
transactional storage layer.

## Storage path

`VFS -> FAT32 -> AHCI -> SATA disk`

AHCI uses polling and `WRITE DMA EXT`, with the same maximum of 128 sectors per
command as the read path. FAT32 updates every configured FAT copy.

## FAT32 write API

- `fat32_write_file(path, buffer, size)` creates or overwrites an 8.3 file.
- `fat32_mkdir(path)` creates one 8.3 directory below an existing directory.

The implementation allocates and zeroes data clusters, links them in the FAT,
then writes the directory entry. Overwriting a file replaces its cluster chain
only after the new directory entry has been persisted.

## VFS integration

The `/mnt/disk0` FAT32 mount is now read-write. When the current VFS path
resolves to a writable FAT32 mount:

- `mkdir` uses `fat32_mkdir`;
- `touch` creates an empty FAT32 file;
- `write` writes the command's text into the FAT32 file.

The RAM-backed VFS remains unchanged for non-FAT32 paths.

## Intentional limits

- FAT32 only, 512-byte sectors.
- Short 8.3 names only; no LFN creation.
- No journaling or crash-consistent transactions.
- No unlink/rename implementation for FAT32 yet.
- Maximum single file write is 16 MiB.
- No executable/ELF installation or userspace launch.
- No permissions/ownership model.
- FSInfo free-cluster accounting is not updated.

Because this milestone performs real disk writes, it should be exercised on a
throwaway/test disk image before being used on valuable data.
