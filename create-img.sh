#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

RED=$'\033[31m'; GREEN=$'\033[32m'; CYAN=$'\033[36m'; YELLOW=$'\033[33m'; BOLD=$'\033[1m'; RESET=$'\033[0m'
if [[ ! -t 1 ]]; then RED=''; GREEN=''; CYAN=''; YELLOW=''; BOLD=''; RESET=''; fi

OUT_IMG="${NEXUS_IMAGE:-NexusOS.img}"
USERDATA_SPEC=""

usage() {
    cat <<USAGE
Usage: $(basename "$0") [options]

Options:
  --userdata SIZE   USERDATA size: 512M, 1G, 2G, 4G, or custom (e.g. 768M)
  --out PATH        output image (default: $OUT_IMG)
  --help            show this help

Without --userdata, an interactive size menu is shown on a terminal.
USAGE
}

die() { printf '%s%sERROR:%s %s\n' "$BOLD" "$RED" "$RESET" "$*" >&2; exit 1; }
info() { printf '%s=>%s %s\n' "$CYAN" "$RESET" "$*"; }
ok() { printf '%s✓%s %s\n' "$GREEN" "$RESET" "$*"; }
warn() { printf '%s!%s %s\n' "$YELLOW" "$RESET" "$*"; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --userdata)
            [[ $# -ge 2 ]] || die "--userdata requires a value"
            USERDATA_SPEC="$2"
            shift 2
            ;;
        --out)
            [[ $# -ge 2 ]] || die "--out requires a path"
            OUT_IMG="$2"
            shift 2
            ;;
        --help|-h) usage; exit 0 ;;
        *) die "Unknown option: $1" ;;
    esac
done

if [[ -z "$USERDATA_SPEC" ]]; then
    if [[ -t 0 ]]; then
        printf '%s%sNexusOS Image Creator%s\n\n' "$BOLD" "$CYAN" "$RESET"
        printf 'Select USERDATA size:\n'
        printf '  1) 512M\n'
        printf '  2) 1G\n'
        printf '  3) 2G\n'
        printf '  4) 4G\n'
        printf '  5) Custom\n\n'
        read -r -p 'Choice [2]: ' choice
        choice="${choice:-2}"
        case "$choice" in
            1) USERDATA_SPEC='512M' ;;
            2) USERDATA_SPEC='1G' ;;
            3) USERDATA_SPEC='2G' ;;
            4) USERDATA_SPEC='4G' ;;
            5) read -r -p 'Custom size (e.g. 768M, 6G): ' USERDATA_SPEC ;;
            *) die 'Invalid choice' ;;
        esac
    else
        USERDATA_SPEC='1G'
        warn "Non-interactive mode: using USERDATA=1G"
    fi
fi

command -v python3 >/dev/null 2>&1 || die 'python3 is required to create the real GPT/FAT32 image'
[[ -f build/BOOTX64.EFI ]] || die 'build/BOOTX64.EFI is missing; run ./build.sh first'
[[ -f build/kernel.elf ]] || die 'build/kernel.elf is missing; run ./build.sh first'
[[ -f iso/EFI/BOOT/BOOTX64.EFI ]] || die 'iso/EFI/BOOT/BOOTX64.EFI is missing; run make iso or ./build.sh first'
[[ -f iso/kernel.elf ]] || die 'iso/kernel.elf is missing; run make iso or ./build.sh first'

mkdir -p "$(dirname "$OUT_IMG")"

info "Creating GPT image: $OUT_IMG"
info "BOOT=64 MiB | SYSTEM=64 MiB | USERDATA=$USERDATA_SPEC"

export NEXUS_ROOT_DIR="$ROOT_DIR"
export NEXUS_OUT_IMG="$OUT_IMG"
export NEXUS_USERDATA_SPEC="$USERDATA_SPEC"

python3 - <<'PY'
import math, os, shutil, struct, sys, uuid, zlib, hashlib
from pathlib import Path

ROOT = Path(os.environ['NEXUS_ROOT_DIR'])
OUT = Path(os.environ['NEXUS_OUT_IMG'])
USERDATA_SPEC = os.environ['NEXUS_USERDATA_SPEC']
SECTOR = 512
ALIGN = 2048
GPT_ENTRIES = 128
GPT_ENTRY_SIZE = 128
GPT_ENTRIES_SECTORS = (GPT_ENTRIES * GPT_ENTRY_SIZE + SECTOR - 1) // SECTOR
GPT_PRIMARY_ENTRIES_LBA = 2
GPT_BACKUP_ENTRIES_LBA_OFFSET = GPT_ENTRIES_SECTORS
GPT_HEADER_SIZE = 92

ESP_GUID = uuid.UUID('C12A7328-F81F-11D2-BA4B-00A0C93EC93B').bytes_le
BASIC_DATA_GUID = uuid.UUID('EBD0A0A2-B9E5-4433-87C0-68B6B72699C7').bytes_le


def die(msg):
    print(f'ERROR: {msg}', file=sys.stderr)
    sys.exit(1)


def parse_size(s):
    s = s.strip().upper()
    units = {'K': 1024, 'M': 1024**2, 'G': 1024**3, 'T': 1024**4}
    if s.isdigit():
        value = int(s) * 1024**2
    else:
        unit = s[-1:] if s else ''
        if unit not in units or not s[:-1].isdigit():
            die(f'invalid USERDATA size: {s!r}')
        value = int(s[:-1]) * units[unit]
    if value < 512 * 1024 * 1024:
        die('USERDATA must be at least 512 MiB')
    if value % SECTOR:
        die('USERDATA size must be sector aligned')
    return value


def align_up(n, a):
    return (n + a - 1) // a * a


def guid_le(g):
    return g.bytes_le


def gpt_partition_entry(type_guid, unique_guid, first, last, name):
    e = bytearray(GPT_ENTRY_SIZE)
    e[0:16] = type_guid
    e[16:32] = unique_guid
    struct.pack_into('<QQQ', e, 32, first, last, 0)
    e[56:128] = name.encode('utf-16le')[:72].ljust(72, b'\0')
    return bytes(e)


def gpt_header(current, backup, first_usable, last_usable, disk_guid, entries_lba, entries_crc):
    h = bytearray(SECTOR)
    h[0:8] = b'EFI PART'
    struct.pack_into('<I', h, 8, 0x00010000)
    struct.pack_into('<I', h, 12, GPT_HEADER_SIZE)
    struct.pack_into('<I', h, 16, 0)
    struct.pack_into('<I', h, 20, 0)
    struct.pack_into('<QQQQ', h, 24, current, backup, first_usable, last_usable)
    h[56:72] = disk_guid
    struct.pack_into('<QIII', h, 72, entries_lba, GPT_ENTRIES, GPT_ENTRY_SIZE, entries_crc)
    crc = zlib.crc32(h[:GPT_HEADER_SIZE]) & 0xffffffff
    struct.pack_into('<I', h, 16, crc)
    return bytes(h)


def fat_params(total_sectors):
    # 64 MiB partitions use 512-byte clusters; larger partitions use 4 KiB clusters.
    spc = 1 if total_sectors <= 524288 else 8
    reserved = 32
    fats = 2
    fat_sectors = 1
    for _ in range(32):
        clusters = (total_sectors - reserved - fats * fat_sectors) // spc
        new_fat = math.ceil((clusters + 2) * 4 / SECTOR)
        if new_fat == fat_sectors:
            break
        fat_sectors = new_fat
    clusters = (total_sectors - reserved - fats * fat_sectors) // spc
    if clusters < 65525:
        die(f'partition too small for FAT32 ({clusters} clusters)')
    return spc, reserved, fats, fat_sectors, clusters


class FAT32:
    def __init__(self, fp, start_lba, sectors, label):
        self.fp = fp
        self.start = start_lba
        self.sectors = sectors
        self.label = label[:11].upper()
        self.spc, self.reserved, self.fats, self.fat_sectors, self.cluster_count = fat_params(sectors)
        self.data_start = self.start + self.reserved + self.fats * self.fat_sectors
        self.next_cluster = 2
        self.dirs = {'/': 2}
        self._write_boot_sector()
        self._write_fsinfo()
        self._write_fats_initial()
        self._zero_cluster(2)
        self._write_dir_entry(2, 0, self._short_entry(self.label, 0x08, 0, 0, 0))
        self._set_fat(2, 0x0FFFFFFF)
        self.next_cluster = 3

    def _seek(self, lba):
        self.fp.seek(lba * SECTOR)

    def _write_boot_sector(self):
        b = bytearray(SECTOR)
        b[0:3] = b'\xEB\x58\x90'
        b[3:11] = b'NEXUSOS '
        struct.pack_into('<H', b, 11, SECTOR)
        b[13] = self.spc
        struct.pack_into('<H', b, 14, self.reserved)
        b[16] = self.fats
        struct.pack_into('<H', b, 17, 0)
        struct.pack_into('<H', b, 19, 0)
        b[21] = 0xF8
        struct.pack_into('<H', b, 22, 0)
        struct.pack_into('<H', b, 24, 63)
        struct.pack_into('<H', b, 26, 255)
        struct.pack_into('<I', b, 28, 0)
        struct.pack_into('<I', b, 32, self.sectors)
        struct.pack_into('<I', b, 36, self.fat_sectors)
        struct.pack_into('<H', b, 40, 0)
        struct.pack_into('<H', b, 42, 0)
        struct.pack_into('<I', b, 44, 2)
        struct.pack_into('<H', b, 48, 1)
        struct.pack_into('<H', b, 50, 6)
        b[64] = 0x80
        b[66] = 0x29
        struct.pack_into('<I', b, 67, zlib.crc32(self.label.encode()) & 0xffffffff)
        b[71:82] = self.label.encode().ljust(11, b' ')
        b[82:90] = b'FAT32   '
        b[510:512] = b'\x55\xAA'
        self._seek(self.start)
        self.fp.write(b)
        backup = bytearray(b)
        self._seek(self.start + 6)
        self.fp.write(backup)

    def _write_fsinfo(self):
        b = bytearray(SECTOR)
        struct.pack_into('<I', b, 0, 0x41615252)
        struct.pack_into('<I', b, 484, 0x61417272)
        struct.pack_into('<I', b, 488, self.cluster_count - 1)
        struct.pack_into('<I', b, 492, 3)
        struct.pack_into('<I', b, 508, 0xAA550000)
        self._seek(self.start + 1)
        self.fp.write(b)
        self._seek(self.start + 7)
        self.fp.write(b)

    def _fat_lba(self, fat_index=0):
        return self.start + self.reserved + fat_index * self.fat_sectors

    def _set_fat(self, cluster, value):
        off = cluster * 4
        for i in range(self.fats):
            lba = self._fat_lba(i) + off // SECTOR
            pos = off % SECTOR
            self._seek(lba)
            data = bytearray(self.fp.read(SECTOR)) if False else bytearray(SECTOR)
            # Read-modify-write without relying on prior zero reads.
            self.fp.seek(lba * SECTOR)
            old = self.fp.read(SECTOR)
            if len(old) != SECTOR:
                old = bytes(SECTOR)
            data[:] = old
            struct.pack_into('<I', data, pos, value & 0x0FFFFFFF)
            self.fp.seek(lba * SECTOR)
            self.fp.write(data)

    def _write_fats_initial(self):
        zero = bytes(SECTOR)
        total = self.fats * self.fat_sectors
        for i in range(total):
            self._seek(self.start + self.reserved + i)
            self.fp.write(zero)
        self._set_fat(0, 0x0FFFFFF8)
        self._set_fat(1, 0xFFFFFFFF)

    def _cluster_lba(self, cluster):
        return self.data_start + (cluster - 2) * self.spc

    def _zero_cluster(self, cluster):
        zero = bytes(SECTOR * self.spc)
        self._seek(self._cluster_lba(cluster))
        self.fp.write(zero)

    def _alloc(self):
        c = self.next_cluster
        self.next_cluster += 1
        if c >= self.cluster_count + 2:
            die(f'FAT32 partition {self.label} is full')
        self._set_fat(c, 0x0FFFFFFF)
        self._zero_cluster(c)
        return c

    @staticmethod
    def _name83(name):
        original = name
        name = name.upper()
        if name in ('.', '..'):
            return name.ljust(11)
        if '.' in name:
            base, ext = name.rsplit('.', 1)
        else:
            base, ext = name, ''
        base = base.replace(' ', '')
        ext = ext.replace(' ', '')
        allowed = set("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$%'-_@~`!(){}^#&")
        if not base or len(ext) > 3:
            die(f'invalid FAT32 filename: {original}')
        if len(base) > 8:
            base = base[:6] + '~1'
        if any(ch not in allowed for ch in base + ext):
            die(f'unsupported FAT32 filename: {original}')
        return base.ljust(8) + ext.ljust(3)

    @staticmethod
    def _lfn_checksum(short11):
        checksum = 0
        for b in short11.encode('ascii'):
            checksum = (((checksum & 1) << 7) | (checksum >> 1)) + b
            checksum &= 0xFF
        return checksum

    def _lfn_entries(self, name, short11):
        chars = list(name)
        count = max(1, math.ceil(len(chars) / 13))
        checksum = self._lfn_checksum(short11)
        result = []
        for ordinal in range(count, 0, -1):
            chunk = chars[(ordinal - 1) * 13:ordinal * 13]
            e = bytearray(32)
            e[0] = ordinal | (0x40 if ordinal == count else 0)
            e[11] = 0x0F
            e[12] = 0
            e[13] = checksum
            e[26:28] = b'\0\0'
            positions = ((1, 5), (14, 6), (28, 2))
            pos = 0
            for offset, amount in positions:
                for j in range(amount):
                    if pos < len(chunk):
                        value = ord(chunk[pos])
                    elif pos == len(chunk) and ordinal == 1:
                        value = 0x0000
                    else:
                        value = 0xFFFF
                    struct.pack_into('<H', e, offset + j * 2, value)
                    pos += 1
            result.append(bytes(e))
        return result

    def _short_entry(self, name, attr, cluster, size, checksum=0):
        e = bytearray(32)
        if attr == 0x08:
            label = name.upper().encode('ascii')[:11]
            e[0:11] = label.ljust(11, b' ')
        else:
            e[0:11] = self._name83(name).encode('ascii')
        e[11] = attr
        struct.pack_into('<H', e, 20, (cluster >> 16) & 0xffff)
        struct.pack_into('<H', e, 26, cluster & 0xffff)
        struct.pack_into('<I', e, 28, size)
        return bytes(e)

    def _write_dir_entry(self, dir_cluster, index, entry):
        entries_per_cluster = self.spc * SECTOR // 32
        if index >= entries_per_cluster:
            die('directory is too large for the current FAT32 builder')
        lba = self._cluster_lba(dir_cluster) + (index * 32) // SECTOR
        pos = (index * 32) % SECTOR
        self._seek(lba)
        old = self.fp.read(SECTOR)
        if len(old) != SECTOR:
            old = bytes(SECTOR)
        b = bytearray(old)
        b[pos:pos+32] = entry
        self._seek(lba)
        self.fp.write(b)

    def _find_free_entries(self, dir_cluster, count):
        entries = self.spc * SECTOR // 32
        for first in range(entries - count + 1):
            free = True
            for i in range(count):
                idx = first + i
                self._seek(self._cluster_lba(dir_cluster) + (idx * 32) // SECTOR)
                sec = self.fp.read(SECTOR)
                pos = (idx * 32) % SECTOR
                if sec[pos] not in (0x00, 0xE5):
                    free = False
                    break
            if free:
                return first
        die('directory cluster is full')

    def _write_entries(self, dir_cluster, start_index, entries):
        for offset, entry in enumerate(entries):
            self._write_dir_entry(dir_cluster, start_index + offset, entry)

    def mkdir(self, path):
        path = path.strip('/')
        if not path:
            return 2
        cur = 2
        built = ''
        for part in path.split('/'):
            built = built + '/' + part
            if built in self.dirs:
                cur = self.dirs[built]
                continue
            c = self._alloc()
            self.dirs[built] = c
            # . and ..
            self._write_dir_entry(c, 0, self._short_entry('.', 0x10, c, 0))
            self._write_dir_entry(c, 1, self._short_entry('..', 0x10, cur, 0))
            short11 = self._name83(part)
            short_display = short11.strip().replace(' ', '')
            long_entries = self._lfn_entries(part, short11) if part.upper() != short_display else []
            idx = self._find_free_entries(cur, len(long_entries) + 1)
            self._write_entries(cur, idx, long_entries + [self._short_entry(part, 0x10, c, 0)])
            cur = c
        return cur

    def add_file(self, path, src):
        path = path.strip('/')
        parent, name = path.rsplit('/', 1) if '/' in path else ('', path)
        parent_cluster = self.mkdir(parent)
        data = Path(src).read_bytes()
        if data:
            clusters = math.ceil(len(data) / (self.spc * SECTOR))
            first = None
            prev = None
            offset = 0
            for _ in range(clusters):
                c = self._alloc()
                if first is None:
                    first = c
                if prev is not None:
                    self._set_fat(prev, c)
                prev = c
                chunk = data[offset:offset + self.spc * SECTOR]
                offset += len(chunk)
                self._seek(self._cluster_lba(c))
                self.fp.write(chunk.ljust(self.spc * SECTOR, b'\0'))
            self._set_fat(prev, 0x0FFFFFFF)
            first_cluster = first
        else:
            first_cluster = 0
        short11 = self._name83(name)
        short_display = short11.strip().replace(' ', '')
        long_entries = self._lfn_entries(name, short11) if name.upper() != short_display else []
        idx = self._find_free_entries(parent_cluster, len(long_entries) + 1)
        self._write_entries(parent_cluster, idx, long_entries + [self._short_entry(name, 0x20, first_cluster, len(data))])


def read_partition_table(fp):
    fp.seek(SECTOR)
    h = fp.read(SECTOR)
    if h[0:8] != b'EFI PART':
        die('primary GPT header signature missing')
    stored_crc = struct.unpack_from('<I', h, 16)[0]
    chk = bytearray(h[:GPT_HEADER_SIZE])
    struct.pack_into('<I', chk, 16, 0)
    if zlib.crc32(chk) & 0xffffffff != stored_crc:
        die('primary GPT header CRC mismatch')
    entries_lba = struct.unpack_from('<Q', h, 72)[0]
    count = struct.unpack_from('<I', h, 80)[0]
    size = struct.unpack_from('<I', h, 84)[0]
    fp.seek(entries_lba * SECTOR)
    raw = fp.read(count * size)
    entries_crc = struct.unpack_from('<I', h, 88)[0]
    if zlib.crc32(raw) & 0xffffffff != entries_crc:
        die('GPT partition-entry CRC mismatch')
    parts = []
    for i in range(count):
        e = raw[i*size:(i+1)*size]
        if e[:16] == bytes(16):
            continue
        typ = e[:16]
        first, last = struct.unpack_from('<QQ', e, 32)
        name = e[56:128].decode('utf-16le').rstrip('\0')
        parts.append((name, typ, first, last))
    return h, parts


userdata_bytes = parse_size(USERDATA_SPEC)
userdata_sectors = userdata_bytes // SECTOR
boot_sectors = 64 * 1024 * 1024 // SECTOR
system_sectors = 64 * 1024 * 1024 // SECTOR

boot_start = ALIGN
system_start = align_up(boot_start + boot_sectors, ALIGN)
userdata_start = align_up(system_start + system_sectors, ALIGN)
userdata_end = userdata_start + userdata_sectors - 1
last_usable = align_up(userdata_end + 1, ALIGN) - 1
# Leave 1 MiB after the last partition for the backup GPT structures.
total_sectors = align_up(userdata_end + GPT_ENTRIES_SECTORS + 34, ALIGN)
last_usable = total_sectors - GPT_ENTRIES_SECTORS - 2
if userdata_end > last_usable:
    total_sectors = align_up(userdata_end + GPT_ENTRIES_SECTORS + 34, ALIGN)
    last_usable = total_sectors - GPT_ENTRIES_SECTORS - 2

parts = [
    ('BOOT', ESP_GUID, boot_start, boot_start + boot_sectors - 1),
    ('SYSTEM', BASIC_DATA_GUID, system_start, system_start + system_sectors - 1),
    ('USERDATA', BASIC_DATA_GUID, userdata_start, userdata_end),
]
if any(p[3] > last_usable for p in parts):
    die('partition layout exceeds GPT usable area')

# Avoid overwriting an existing image accidentally.
if OUT.exists():
    OUT.unlink()
OUT.parent.mkdir(parents=True, exist_ok=True)

print(f'Image: {OUT}')
print(f'GPT sectors: {total_sectors:,} ({total_sectors*SECTOR/1024/1024:.0f} MiB)')
for name, _, first, last in parts:
    print(f'  {name:<8} {first:>10}..{last:<10} {(last-first+1)*SECTOR/1024/1024:.0f} MiB')

with OUT.open('w+b') as fp:
    fp.truncate(total_sectors * SECTOR)
    disk_guid = uuid.uuid4().bytes_le
    entries = bytearray(GPT_ENTRIES * GPT_ENTRY_SIZE)
    for i, (name, typ, first, last) in enumerate(parts):
        entry = gpt_partition_entry(typ, uuid.uuid4().bytes_le, first, last, name)
        entries[i*GPT_ENTRY_SIZE:(i+1)*GPT_ENTRY_SIZE] = entry
    entries_crc = zlib.crc32(entries) & 0xffffffff
    primary = gpt_header(1, total_sectors - 1, 34, last_usable, disk_guid, GPT_PRIMARY_ENTRIES_LBA, entries_crc)
    backup_entries_lba = total_sectors - GPT_ENTRIES_SECTORS - 1
    backup = gpt_header(total_sectors - 1, 1, 34, last_usable, disk_guid, backup_entries_lba, entries_crc)

    # Protective MBR.
    mbr = bytearray(SECTOR)
    mbr[0:3] = b'\xFA\xEB\xFE'
    mbr[446:462] = bytes([0,0,2,0,0xEE,0xFF,0xFF,0xFF]) + struct.pack('<I', 1) + struct.pack('<I', min(total_sectors-1, 0xFFFFFFFF))
    mbr[510:512] = b'\x55\xAA'
    fp.seek(0); fp.write(mbr)
    fp.seek(SECTOR); fp.write(primary)
    fp.seek(GPT_PRIMARY_ENTRIES_LBA * SECTOR); fp.write(entries)
    fp.seek(backup_entries_lba * SECTOR); fp.write(entries)
    fp.seek((total_sectors - 1) * SECTOR); fp.write(backup)

    boot = FAT32(fp, boot_start, boot_sectors, 'NEXUSBOOT')
    boot.add_file('EFI/BOOT/BOOTX64.EFI', ROOT / 'iso/EFI/BOOT/BOOTX64.EFI')
    # Current NexusOS UEFI loader intentionally loads \kernel.elf from the
    # same EFI filesystem volume it was started from.
    boot.add_file('kernel.elf', ROOT / 'iso/kernel.elf')

    system = FAT32(fp, system_start, system_sectors, 'NEXUSSYSTEM')
    # Keep a real SYSTEM copy while preserving the existing bootloader path.
    system.add_file('KERNEL/KERNEL.ELF', ROOT / 'build/kernel.elf')
    system.add_file('BOOT/BOOTX64.EFI', ROOT / 'build/BOOTX64.EFI')

    user = FAT32(fp, userdata_start, userdata_sectors, 'NEXUSERDATA')
    src = ROOT / 'userdata'
    if src.exists():
        for p in sorted(src.rglob('*')):
            rel = p.relative_to(src).as_posix()
            if p.is_dir():
                user.mkdir(rel)
            elif p.is_file() and p.name.upper() in ('README.MD',):
                user.add_file(rel, p)

# Verification: reopen and validate GPT + partition geometry + FAT32 VBRs + required files.
with OUT.open('rb') as fp:
    _, parsed = read_partition_table(fp)
    if [p[0] for p in parsed[:3]] != ['BOOT', 'SYSTEM', 'USERDATA']:
        die('GPT partition names do not match expected layout')
    expected = {'BOOT': boot_sectors, 'SYSTEM': system_sectors, 'USERDATA': userdata_sectors}
    for name, typ, first, last in parsed[:3]:
        if (last-first+1) != expected[name]:
            die(f'{name} partition size verification failed')
        fp.seek(first * SECTOR)
        vbr = fp.read(SECTOR)
        if vbr[510:512] != b'\x55\xAA' or vbr[3:11] != b'NEXUSOS ':
            die(f'{name}: FAT32 VBR verification failed')
        if struct.unpack_from('<H', vbr, 11)[0] != 512:
            die(f'{name}: bytes-per-sector verification failed')

    # Required boot files: verify their FAT32 directory entries by walking 8.3 dirs.
    def read_fat(start, sectors, path):
        fp.seek(start * SECTOR)
        vbr = fp.read(SECTOR)
        spc = vbr[13]
        reserved = struct.unpack_from('<H', vbr, 14)[0]
        fats = vbr[16]
        fat_sz = struct.unpack_from('<I', vbr, 36)[0]
        root = struct.unpack_from('<I', vbr, 44)[0]
        data_start = start + reserved + fats * fat_sz
        fat = bytearray()
        fp.seek((start + reserved) * SECTOR)
        fat = fp.read(fat_sz * SECTOR)
        def chain(c):
            out=[]
            seen=set()
            while c >= 2 and c < 0x0FFFFFF8 and c not in seen:
                seen.add(c); out.append(c)
                off=c*4
                c=struct.unpack_from('<I', fat, off)[0] & 0x0FFFFFFF
            return out
        def entries(c):
            for cc in chain(c):
                fp.seek((data_start + (cc-2)*spc) * SECTOR)
                data=fp.read(spc*SECTOR)
                for off in range(0,len(data),32):
                    e=data[off:off+32]
                    if not e or e[0] == 0: return
                    if e[0] == 0xE5 or e[11] == 0x0F: continue
                    name=e[:8].decode('ascii','ignore').rstrip(' ')
                    ext=e[8:11].decode('ascii','ignore').rstrip(' ')
                    full=name + ('.'+ext if ext else '')
                    cl=(struct.unpack_from('<H',e,20)[0]<<16)|struct.unpack_from('<H',e,26)[0]
                    size=struct.unpack_from('<I',e,28)[0]
                    yield full, e[11], cl, size
        c=root
        for comp in [x for x in path.strip('/').split('/') if x]:
            found=None
            for name, attr, cl, size in entries(c) or []:
                if name.upper() == comp.upper():
                    found=(attr,cl,size); break
            if found is None: return None
            attr,c,size=found
        return size

    boot_p = next(p for p in parsed if p[0]=='BOOT')
    system_p = next(p for p in parsed if p[0]=='SYSTEM')
    user_p = next(p for p in parsed if p[0]=='USERDATA')
    if read_fat(*[boot_p[2], boot_p[3]-boot_p[2]+1], 'EFI/BOOT/BOOTX64.EFI') is None:
        die('BOOT/EFI/BOOT/BOOTX64.EFI missing from image')
    if read_fat(boot_p[2], boot_p[3]-boot_p[2]+1, 'kernel.elf') is None:
        die('BOOT/kernel.elf missing from image')
    if read_fat(system_p[2], system_p[3]-system_p[2]+1, 'KERNEL/KERNEL.ELF') is None:
        die('SYSTEM/KERNEL/KERNEL.ELF missing from image')
    if read_fat(user_p[2], user_p[3]-user_p[2]+1, 'README.MD') is None:
        die('USERDATA/README.MD missing from image')

# Source/image hashes for boot-critical files.
def sha256(p):
    h=hashlib.sha256()
    with open(p,'rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):
            h.update(chunk)
    return h.hexdigest()

if sha256(ROOT/'build/BOOTX64.EFI') != sha256(ROOT/'iso/EFI/BOOT/BOOTX64.EFI'):
    die('BOOTX64.EFI differs between build/ and iso/')
if sha256(ROOT/'build/kernel.elf') != sha256(ROOT/'iso/kernel.elf'):
    die('kernel.elf differs between build/ and iso/')

print('validated: GPT + 3 partitions + FAT32 VBRs + required NexusOS files')
PY

ok "Image verified: $OUT_IMG"
printf '%s\n' "  BOOT     64 MiB  FAT32  EFI System Partition" "  SYSTEM   64 MiB  FAT32" "  USERDATA $USERDATA_SPEC FAT32" "  Kernel   BOOT/kernel.elf (existing UEFI bootloader compatibility)" "  System   SYSTEM/KERNEL/KERNEL.ELF (real kernel mirror)"
