#!/usr/bin/env bash
# NexusOS — create-img.sh
# Собирает FAT32-образ диска (build/fat.img) из готового iso/.
# Если iso/ ещё не существует — сначала запускает сборку через build.sh.
#
# Использование:
#   ./create-img.sh              # образ 64 МБ (по умолчанию)
#   ./create-img.sh --size 128   # образ 128 МБ
#   ./create-img.sh --out my.img # другой путь к образу
#   ./create-img.sh --help

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

RESET='\033[0m'; BOLD='\033[1m'; CYAN='\033[36m'
GREEN='\033[32m'; RED='\033[31m'; YELLOW='\033[33m'; GRAY='\033[90m'
[[ -t 1 ]] || { RESET=''; BOLD=''; CYAN=''; GREEN=''; RED=''; YELLOW=''; GRAY=''; }

die()  { printf '%s%sERROR:%s %s\n' "$BOLD" "$RED" "$RESET" "$*" >&2; exit 1; }
info() { printf '%s=>%s %s\n' "$CYAN" "$RESET" "$*"; }
ok()   { printf '%s✓%s  %s\n' "$GREEN" "$RESET" "$*"; }
warn() { printf '%s!%s  %s\n' "$YELLOW" "$RESET" "$*"; }

IMG_SIZE_MB=64
OUT_IMG="build/fat.img"

usage() {
    printf 'Usage: %s [options]\n\n' "$(basename "$0")"
    printf 'Options:\n'
    printf '  --size  MB    FAT32 image size in MiB (default: %d)\n' "$IMG_SIZE_MB"
    printf '  --out   PATH  output image path       (default: %s)\n' "$OUT_IMG"
    printf '  --help        show this help\n'
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --size)  [[ -n "${2:-}" ]] || die "--size requires a value"; IMG_SIZE_MB="$2"; shift 2 ;;
        --out)   [[ -n "${2:-}" ]] || die "--out requires a value";  OUT_IMG="$2";     shift 2 ;;
        --help|-h) usage ;;
        *) die "Unknown option: $1" ;;
    esac
done

[[ "$IMG_SIZE_MB" =~ ^[0-9]+$ && "$IMG_SIZE_MB" -ge 8 ]] \
    || die "Image size must be a number ≥ 8 MiB (got: $IMG_SIZE_MB)"

printf '%s%sNexusOS Image Creator%s\n\n' "$BOLD" "$CYAN" "$RESET"

# --- зависимости ---
for cmd in dd mkfs.vfat mcopy mmd; do
    command -v "$cmd" &>/dev/null \
        || die "Required tool not found: $cmd  (install dosfstools + mtools)"
done

# --- сборка если нужно ---
if [[ ! -f iso/EFI/BOOT/BOOTX64.EFI || ! -f iso/kernel.elf ]]; then
    warn "iso/ is missing or incomplete — running build.sh first"
    bash build.sh || die "Build failed; fix errors and retry"
fi

ok "iso/ looks good"

# --- образ ---
mkdir -p "$(dirname "$OUT_IMG")"

info "Creating blank FAT32 image: ${OUT_IMG} (${IMG_SIZE_MB} MiB)"
dd if=/dev/zero of="$OUT_IMG" bs=1M count="$IMG_SIZE_MB" status=none

info "Formatting as FAT32"
mkfs.vfat -F 32 -n "NEXUSOS" "$OUT_IMG" >/dev/null

info "Copying ESP structure (iso/*) into image"
# mmd нужен если mcopy не создаёт директории рекурсивно на пустом образе
mmd -i "$OUT_IMG" ::/EFI ::/EFI/BOOT 2>/dev/null || true
mcopy -i "$OUT_IMG" -s iso/* ::/ \
    || die "mcopy failed — is the image large enough? (current: ${IMG_SIZE_MB} MiB)"

printf '\n'
ok "Image ready: ${OUT_IMG}"
printf '  %sSize:%s    %d MiB\n' "$GRAY" "$RESET" "$IMG_SIZE_MB"
printf '  %sEFI:%s     EFI/BOOT/BOOTX64.EFI\n' "$GRAY" "$RESET"
printf '  %sKernel:%s  kernel.elf\n' "$GRAY" "$RESET"
printf '\nNext: %s./run.sh%s\n' "$CYAN" "$RESET"
