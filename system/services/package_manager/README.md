# NexusOS Package Manager

NexusOS 0.5.14 extends the writable installation transaction with a bounded
multi-file application payload layout.

## Manifest

A package directory contains `manifest.nxm` and payload files. `Name` and `ID`
are required. `Entry` is optional. `Files` is an optional comma/semicolon/
whitespace-separated list of payload filenames.

Example:

```text
Name: Demo App
ID: demo.app
Version: 1.0
Entry: APP.ELF
Files: APP.ELF DATA.BIN ICON.BIN
```

When `Files` is present, every listed file is copied. `Entry`, when present,
must name one of those files. Without `Files`, the legacy single-`Entry`
installation path is retained.

## Installation

1. Validate the manifest and every payload before changing the destination.
2. Create the destination directory.
3. Copy payload files in manifest order.
4. Write `manifest.nxm` last; this remains the publication/commit point used by
discovery.
5. On failure, remove copied payloads in reverse order and remove the empty
staging directory.

## Limits

- FAT32 8.3 names only.
- Maximum 8 payload files per installation.
- Each payload must be smaller than 64 KiB because the current FAT32 read API
  is bounded and has no offset/streaming interface.
- No archive extraction, permissions, signatures, dependency resolution, LFN,
  or ELF execution yet.
