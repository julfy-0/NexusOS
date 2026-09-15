#!/usr/bin/env bash
# NexusOS QEMU launcher
# Boots the existing NexusOS UEFI image without changing OS sources.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

DISK_IMG="build/fat.img"
ISO_IMG=""
MEM_MB=256
USE_KVM=1
REBUILD=0
FULLSCREEN=1

RED=$'\033[31m'; GREEN=$'\033[32m'; CYAN=$'\033[36m'; YELLOW=$'\033[33m'; BOLD=$'\033[1m'; RESET=$'\033[0m'; GRAY=$'\033[90m'
if [[ ! -t 1 ]]; then RED=''; GREEN=''; CYAN=''; YELLOW=''; BOLD=''; RESET=''; GRAY=''; fi

die() { printf '%s%sERROR:%s %s\n' "$BOLD" "$RED" "$RESET" "$*" >&2; exit 1; }
info() { printf '%s=>%s %s\n' "$CYAN" "$RESET" "$*"; }
ok() { printf '%s✓%s %s\n' "$GREEN" "$RESET" "$*"; }
warn() { printf '%s!%s %s\n' "$YELLOW" "$RESET" "$*"; }

usage() {
    cat <<USAGE
NexusOS QEMU launcher

Usage: ./run.sh [options]

Options:
  --rebuild       recreate NexusOS.img before booting (disk-image mode)
  --iso PATH       boot a bootable NexusOS ISO directly
  --no-kvm        disable KVM acceleration
  --mem MB        guest RAM in MiB (default: $MEM_MB)
  --img PATH      disk image path (default: $DISK_IMG)
  --window        use a window instead of fullscreen
  --help          show this help

Environment:
  OVMF_CODE=PATH  use a specific OVMF_CODE firmware file
  OVMF_VARS=PATH  use a specific writable OVMF variables file
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --rebuild) REBUILD=1; shift ;;
        --no-kvm) USE_KVM=0; shift ;;
        --mem)
            [[ $# -ge 2 ]] || die '--mem requires a value'
            MEM_MB="$2"
            shift 2
            ;;
        --img)
            [[ $# -ge 2 ]] || die '--img requires a path'
            DISK_IMG="$2"
            shift 2
            ;;
        --iso)
            [[ $# -ge 2 ]] || die '--iso requires a path'
            ISO_IMG="$2"
            shift 2
            ;;
        --window) FULLSCREEN=0; shift ;;
        --help|-h) usage; exit 0 ;;
        *) die "Unknown option: $1" ;;
    esac
done

[[ "$MEM_MB" =~ ^[0-9]+$ && "$MEM_MB" -ge 64 ]] || die "RAM must be at least 64 MiB (got: $MEM_MB)"
command -v qemu-system-x86_64 >/dev/null 2>&1 || die 'qemu-system-x86_64 not found (install qemu-system-x86)'

find_first_file() {
    local candidate
    for candidate in "$@"; do
        if [[ -f "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done
    return 1
}

# -----------------------------------------------------------------------------
# OVMF discovery
# -----------------------------------------------------------------------------

OVMF_CODE="${OVMF_CODE:-}"
if [[ -z "$OVMF_CODE" ]]; then
    OVMF_CODE="$(find_first_file \
        /usr/share/OVMF/OVMF_CODE_4M.fd \
        /usr/share/OVMF/OVMF_CODE.fd \
        /usr/share/edk2/ovmf/OVMF_CODE.fd \
        /usr/share/edk2-ovmf/OVMF_CODE.fd \
        /usr/share/edk2-ovmf/x64/OVMF_CODE.fd \
        /usr/share/qemu/OVMF.fd \
        "$ROOT_DIR/OVMF_CODE.fd" 2>/dev/null || true)"
fi

# Last-resort discovery for distro-specific OVMF locations.
if [[ -z "$OVMF_CODE" ]]; then
    OVMF_CODE="$(find /usr/share -type f \( \
        -name 'OVMF_CODE_4M.fd' -o \
        -name 'OVMF_CODE.fd' \
    \) -print -quit 2>/dev/null || true)"
fi

[[ -n "$OVMF_CODE" ]] || die 'OVMF_CODE firmware not found (install ovmf or set OVMF_CODE=...)'
[[ -r "$OVMF_CODE" ]] || die "OVMF firmware is not readable: $OVMF_CODE"

OVMF_VARS="${OVMF_VARS:-$ROOT_DIR/OVMF_VARS.fd}"
if [[ ! -f "$OVMF_VARS" ]]; then
    VARS_TEMPLATE="$(find_first_file \
        /usr/share/OVMF/OVMF_VARS_4M.fd \
        /usr/share/OVMF/OVMF_VARS.fd \
        /usr/share/edk2/ovmf/OVMF_VARS.fd \
        /usr/share/edk2-ovmf/OVMF_VARS.fd \
        /usr/share/edk2-ovmf/x64/OVMF_VARS.fd \
        2>/dev/null || true)"
    [[ -n "$VARS_TEMPLATE" ]] || die 'OVMF_VARS template not found; install ovmf'
    mkdir -p "$(dirname "$OVMF_VARS")"
    cp "$VARS_TEMPLATE" "$OVMF_VARS"
    ok "Created OVMF variables file: $OVMF_VARS"
fi

# -----------------------------------------------------------------------------
# Build / image preparation
# -----------------------------------------------------------------------------

[[ -s build/BOOTX64.EFI ]] || die 'build/BOOTX64.EFI is missing; run ./build.sh'
[[ -s build/kernel.elf ]] || die 'build/kernel.elf is missing; run ./build.sh'

if [[ -n "$ISO_IMG" ]]; then
    [[ -s "$ISO_IMG" ]] || die "ISO image is missing or empty: $ISO_IMG"
    ok "Bootable ISO: $ISO_IMG"
else
    if [[ "$REBUILD" -eq 1 || ! -f "$DISK_IMG" ]]; then
        if [[ "$REBUILD" -eq 1 ]]; then
            info "Rebuilding disk image (--rebuild)"
        else
            warn "Disk image not found: $DISK_IMG — creating it now"
        fi
        bash create-img.sh --out "$DISK_IMG" \
            || die "create-img.sh failed"
    fi
    [[ -s "$DISK_IMG" ]] || die "Disk image is missing or empty: $DISK_IMG"
    ok "Disk image: $DISK_IMG"
fi
ok "OVMF:       $OVMF_CODE"

# --- KVM ---
KVM_FLAGS=()
if [[ "$USE_KVM" -eq 1 ]]; then
    if [[ -r /dev/kvm ]]; then
        KVM_FLAGS=(-enable-kvm -cpu host)
        ok "KVM acceleration enabled"
    else
        warn "KVM not available — running without acceleration (slower)"
        USE_KVM=0
    fi
fi
[[ "$USE_KVM" -eq 0 ]] && KVM_FLAGS=(-cpu qemu64)

# --- launch UI / design -------------------------------------------------------
# This section only changes the launcher presentation. The actual QEMU command
# below intentionally remains the classic NexusOS command line.
if [[ -t 1 ]]; then
    CLEAR=$'\033[2J\033[H'
    DIM=$'\033[2m'
    WHITE=$'\033[97m'
    BLUE=$'\033[94m'
    MAGENTA=$'\033[95m'
    GREEN=$'\033[92m'
    YELLOW=$'\033[93m'
    RED=$'\033[91m'
    RESET=$'\033[0m'
else
    CLEAR=''; DIM=''; WHITE=''; BLUE=''; MAGENTA=''; GREEN=''; YELLOW=''; RED=''; RESET=''
fi

printf '%s' "$CLEAR"
printf '\n'
printf '%s╔══════════════════════════════════════════════════════════════╗%s\n' "$BLUE$WHITE" "$RESET"
printf '%s║%s                     %sN E X U S O S%s                    %s║%s\n' "$BLUE" "$RESET" "$MAGENTA$BOLD" "$RESET" "$BLUE" "$RESET"
printf '%s║%s                  QEMU SYSTEM LAUNCHER                  %s║%s\n' "$BLUE" "$DIM" "$BLUE" "$RESET"
printf '%s╠══════════════════════════════════════════════════════════════╣%s\n' "$BLUE" "$RESET"
printf '%s║%s  %-14s %s %-37s %s║%s\n' "$BLUE" "$RESET" 'IMAGE' ':' "$DISK_IMG" "$BLUE" "$RESET"
printf '%s║%s  %-14s %s %-37s %s║%s\n' "$BLUE" "$RESET" 'MEMORY' ':' "${MEM_MB} MiB" "$BLUE" "$RESET"
printf '%s║%s  %-14s %s %-37s %s║%s\n' "$BLUE" "$RESET" 'FIRMWARE' ':' 'UEFI / OVMF' "$BLUE" "$RESET"
if [[ "$USE_KVM" -eq 1 ]]; then
    ACCEL_TEXT='KVM / hardware acceleration'
else
    ACCEL_TEXT='TCG / software acceleration'
fi
printf '%s║%s  %-14s %s %-37s %s║%s\n' "$BLUE" "$RESET" 'ACCELERATION' ':' "$ACCEL_TEXT" "$BLUE" "$RESET"
if [[ "$FULLSCREEN" -eq 1 ]]; then
    MODE_TEXT='Fullscreen / GTK OpenGL'
else
    MODE_TEXT='Windowed / GTK OpenGL'
fi
printf '%s║%s  %-14s %s %-37s %s║%s\n' "$BLUE" "$RESET" 'DISPLAY' ':' "$MODE_TEXT" "$BLUE" "$RESET"
printf '%s╠══════════════════════════════════════════════════════════════╣%s\n' "$BLUE" "$RESET"
printf '%s║%s              %s✓ SYSTEM READY — STARTING...%s              %s║%s\n' "$BLUE" "$RESET" "$GREEN$BOLD" "$RESET" "$BLUE" "$RESET"
printf '%s╚══════════════════════════════════════════════════════════════╝%s\n' "$BLUE" "$RESET"
printf '\n%s%sLaunching NexusOS...%s\n' "$BOLD" "$CYAN" "$RESET"
printf '%s%sKeyboard: USB%s    %sMouse: USB tablet%s\n' "$DIM" "$RESET" "$RESET" "$DIM" "$RESET"
printf '%s%sQEMU window will appear now.%s\n\n' "$DIM" "$RESET" "$RESET"

# Classic NexusOS display flags, unchanged from the pre-rollback launcher.
DISPLAY_FLAGS=()
if [[ "$FULLSCREEN" -eq 1 ]]; then
    DISPLAY_FLAGS=(-full-screen -display gtk,gl=on)
else
    DISPLAY_FLAGS=(-display gtk,gl=on)
fi

# --- OVMF_VARS drive (optional) ---
VARS_DRIVE=()
[[ -n "$OVMF_VARS" && -f "$OVMF_VARS" ]] && VARS_DRIVE=(-drive "if=pflash,format=raw,file=${OVMF_VARS}")

QEMU_STORAGE=()
if [[ -n "$ISO_IMG" ]]; then
    QEMU_STORAGE=(-cdrom "$ISO_IMG")
else
    QEMU_STORAGE=(-drive "format=raw,file=${DISK_IMG}")
fi

exec qemu-system-x86_64 \
    -drive "if=pflash,format=raw,readonly=on,file=${OVMF_CODE}" \
    "${VARS_DRIVE[@]}" \
    "${QEMU_STORAGE[@]}" \
    -m "${MEM_MB}M" \
    -device qemu-xhci,id=xhci \
    -usb \
    -device usb-tablet \
    "${KVM_FLAGS[@]}" \
    "${DISPLAY_FLAGS[@]}"
