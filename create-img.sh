#!/usr/bin/env bash
# NexusOS 0.5.2 — real GPT disk image builder.
# Layout: BOOT 64 MiB FAT32, SYSTEM 64 MiB FAT32, USERDATA configurable FAT32.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; cd "$ROOT_DIR"
RESET='\033[0m'; BOLD='\033[1m'; CYAN='\033[36m'; GREEN='\033[32m'; RED='\033[31m'; YELLOW='\033[33m'; GRAY='\033[90m'
[[ -t 1 ]] || { RESET=''; BOLD=''; CYAN=''; GREEN=''; RED=''; YELLOW=''; GRAY=''; }
die(){ printf '%s%sERROR:%s %s\n' "$BOLD" "$RED" "$RESET" "$*" >&2; exit 1; }
ok(){ printf '%s✓%s  %s\n' "$GREEN" "$RESET" "$*"; }
info(){ printf '%s=>%s %s\n' "$CYAN" "$RESET" "$*"; }
USERDATA=""; OUT="NexusOS.img"
usage(){ cat <<USAGE
Usage: $0 [--userdata SIZE] [--out PATH]
  --userdata 512M|1G|2G|4G|10G...  USERDATA partition size (default: interactive 1G)
  --out PATH                       output image (default: NexusOS.img)
  --help
USAGE
exit 0; }
while [[ $# -gt 0 ]]; do case "$1" in
  --userdata) [[ -n "${2:-}" ]] || die "--userdata requires a value"; USERDATA="$2"; shift 2;;
  --out) [[ -n "${2:-}" ]] || die "--out requires a path"; OUT="$2"; shift 2;;
  --help|-h) usage;; *) die "Unknown option: $1";; esac; done
if [[ -z "$USERDATA" ]]; then
  printf '%s%sNexusOS Image Builder%s\n\n' "$BOLD" "$CYAN" "$RESET"
  printf 'BOOT       64 MiB\nSYSTEM     64 MiB\n\nSelect USERDATA size:\n'
  select choice in 512M 1G 2G 4G Custom; do
    case "$REPLY" in 1|2|3|4) USERDATA="$choice"; break;; 5) read -r -p '> ' USERDATA; break;; esac
  done
fi
python3 - "$USERDATA" <<'PY'
import sys,re
s=sys.argv[1].strip().upper(); m=re.fullmatch(r'(\d+)([KMG])?',s)
if not m: raise SystemExit('Invalid USERDATA size; use e.g. 512M, 1G or 10G')
n=int(m.group(1)); u=m.group(2) or 'M'; mult={'K':1024,'M':1024**2,'G':1024**3}[u]
if n*mult < 512*1024**2: raise SystemExit('USERDATA must be at least 512 MiB')
_ = n*mult//512
PY
USERDATA_SECTORS="$(python3 - "$USERDATA" <<'PY'
import sys,re
s=sys.argv[1].strip().upper(); m=re.fullmatch(r'(\d+)([KMG])?',s); n=int(m.group(1)); mult={'K':1024,'M':1024**2,'G':1024**3}.get(m.group(2) or 'M',1); print(n*mult//512)
PY
)"
[[ -f build/BOOTX64.EFI && -f build/kernel.elf ]] || { info 'Build artifacts missing; running build.sh'; ./build.sh; }
[[ -f tools/create-fat32.py ]] || die 'tools/create-fat32.py missing'
mkdir -p "$(dirname "$OUT")"
info "Creating GPT image: $OUT"
python3 tools/create-fat32.py --out "$OUT" --userdata-sectors "$USERDATA_SECTORS" \
  --bootloader build/BOOTX64.EFI --kernel build/kernel.elf --config system/config/system.conf
ok "GPT image created"
# Dependency-free validation of the exact GPT/FAT32 signatures and partition sizes.
python3 - "$OUT" "$USERDATA_SECTORS" <<'PY'
import sys,struct
p=sys.argv[1]; expected=int(sys.argv[2]); f=open(p,'rb')
f.seek(512); h=f.read(512)
assert h[:8]==b'EFI PART', 'primary GPT header missing'
entries_lba=struct.unpack_from('<Q',h,72)[0]; count=struct.unpack_from('<I',h,80)[0]; size=struct.unpack_from('<I',h,84)[0]
parts=[]
for i in range(3):
 f.seek((entries_lba*512)+i*size); e=f.read(128); parts.append((struct.unpack_from('<Q',e,32)[0],struct.unpack_from('<Q',e,40)[0]))
assert parts[0][1]-parts[0][0]+1==64*1024*1024//512
assert parts[1][1]-parts[1][0]+1==64*1024*1024//512
assert parts[2][1]-parts[2][0]+1==expected
for first,last in parts:
 f.seek(first*512); assert f.read(3)[:2] in (b'\xeb\x58',b'\xeb\x3c'), 'FAT32 VBR missing'
print('validated: GPT + 3 partitions + FAT32 VBRs + requested USERDATA size')
PY
printf '\n%sNexusOS.img ready%s\n' "$BOLD" "$RESET"
printf '  BOOT:     64 MiB FAT32 -> /boot\n  SYSTEM:   64 MiB FAT32 -> /system\n  USERDATA: %s -> /userdata\n' "$USERDATA"
printf '  Kernel:   SYSTEM/KERNEL/KERNEL.ELF\n  EFI:      BOOT/EFI/BOOT/BOOTX64.EFI\n'
