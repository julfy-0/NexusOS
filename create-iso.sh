#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

RED=$'\033[31m'; GREEN=$'\033[32m'; CYAN=$'\033[36m'; YELLOW=$'\033[33m'; BOLD=$'\033[1m'; RESET=$'\033[0m'
if [[ ! -t 1 ]]; then RED=''; GREEN=''; CYAN=''; YELLOW=''; BOLD=''; RESET=''; fi

OUT_ISO="${NEXUS_ISO:-NexusOS.iso}"

usage() {
    cat <<USAGE
Usage: $(basename "$0") [options]

Create a standalone bootable NexusOS UEFI ISO from existing build artifacts.

Options:
  --out PATH        output ISO path (default: $OUT_ISO)
  --help            show this help

Required build artifacts:
  build/BOOTX64.EFI
  build/kernel.elf

The generated ISO contains:
  EFI/BOOT/BOOTX64.EFI
  kernel.elf
  El Torito EFI boot image
  ISO9660 filesystem
USAGE
}

die() { printf '%s%sERROR:%s %s\n' "$BOLD" "$RED" "$RESET" "$*" >&2; exit 1; }
info() { printf '%s=>%s %s\n' "$CYAN" "$RESET" "$*"; }
ok() { printf '%s✓%s %s\n' "$GREEN" "$RESET" "$*"; }
warn() { printf '%s!%s %s\n' "$YELLOW" "$RESET" "$*"; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --out)
            [[ $# -ge 2 ]] || die "--out requires a path"
            OUT_ISO="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            die "Unknown option: $1"
            ;;
    esac
done

command -v python3 >/dev/null 2>&1 || die 'python3 is required to create the bootable ISO'
[[ -f tools/create_iso.py ]] || die 'tools/create_iso.py is missing'
[[ -f build/BOOTX64.EFI ]] || die 'build/BOOTX64.EFI is missing; run ./build.sh first'
[[ -f build/kernel.elf ]] || die 'build/kernel.elf is missing; run ./build.sh first'

OUT_DIR="$(dirname "$OUT_ISO")"
mkdir -p "$OUT_DIR"

# WSL/Windows mounts can leave an older ISO owned or marked read-only
# (for example after a previous sudo invocation). Remove only the target
# file so the normal user can recreate it.
if [[ -e "$OUT_ISO" && ! -w "$OUT_ISO" ]]; then
    warn "Existing ISO is not writable; removing stale target: $OUT_ISO"
    rm -f -- "$OUT_ISO" || die "cannot remove existing ISO: $OUT_ISO (try: sudo rm -f -- '$OUT_ISO')"
fi

[[ -w "$OUT_DIR" ]] || die "output directory is not writable: $OUT_DIR"

info "Creating bootable ISO: $OUT_ISO"
python3 tools/create_iso.py \
    --bootloader build/BOOTX64.EFI \
    --kernel build/kernel.elf \
    --out "$OUT_ISO"

[[ -s "$OUT_ISO" ]] || die "bootable ISO was not created: $OUT_ISO"

if command -v file >/dev/null 2>&1; then
    file_info="$(file -b "$OUT_ISO")"
    if [[ "$file_info" != *"ISO 9660"* ]]; then
        die "generated file is not recognized as ISO9660: $file_info"
    fi
fi

ok "ISO verified: $OUT_ISO"
printf '%s\n' \
    "  EFI      EFI/BOOT/BOOTX64.EFI" \
    "  Kernel   kernel.elf" \
    "  Format   ISO9660 + El Torito EFI"
