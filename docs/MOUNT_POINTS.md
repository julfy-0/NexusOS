# NexusOS VFS mount points

NexusOS has a mount namespace independent from the physical storage drivers.

## Default namespace

```text
/          rootfs    nexusfs
/dev       devfs
/proc      procfs
/sys       sysfs
/tmp       tmpfs
/mnt/disk0  fat32    (created automatically when AHCI + FAT32 are detected)
```

`/mnt/disk0` is a mount-table entry for the existing read-only FAT32 driver; it does not yet make the existing `diskls` path parser operate through VFS.

## Shell commands

```text
mount
mount <source> <target> <fstype>
umount <target>
mounts
```

Examples:

```text
NexusOS> mount
NexusOS> mount nvme0p1 /home fat32
NexusOS> umount /home
```

The namespace supports longest-prefix resolution, so `/dev/usb` resolves to `/dev` while `/dev/usb/hid` remains under the same mount.

The next storage step is to connect the VFS mount layer to real block devices and filesystem drivers (NVMe/AHCI + FAT32/NexusFS).


## 0.5: path traversal

Mounted filesystems are now visible through the normal VFS path API: `cd /mnt/disk0`, `pwd`, `ls`, and `cat /mnt/disk0/<file>` dispatch into the mounted FAT32 backend. The FAT32 backend remains read-only.
