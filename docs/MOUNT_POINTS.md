# NexusOS mount points

The VFS namespace is now aligned with the Nexus System disk layout.

```text
/             rootfs
/dev          devfs
/proc         procfs
/sys          sysfs
/tmp          tmpfs
/boot         BOOT FAT32 partition
/system       SYSTEM FAT32 partition
/userdata     USERDATA FAT32 partition
/mnt/disk0    compatibility alias for BOOT
```

The physical FAT32 implementation is read-only, but multiple FAT32 mount
contexts can be active at once. GPT discovery selects the partition LBA and
assigns a backend slot before registering its VFS mount.

`/userdata` is the persistent user-data namespace. The image builder creates:

```text
/users
/home
/apps
/packages
/downloads
/documents
/config
```

inside USERDATA.
