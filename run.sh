#!/usr/bin/env bash
# NexusOS — run.sh
# Запускает NexusOS в QEMU (OVMF UEFI, x86_64).
# Если build/fat.img отсутствует — сначала запускает create-img.sh.
#
# Использование:
#   ./run.sh                # полноэкранный режим (по умолчанию)
#   ./run.sh --window       # оконный режим
#   ./run.sh --no-kvm       # без KVM (для ВМ внутри ВМ)
#   ./run.sh --mem 512      # объём RAM в МБ (default: 256)
#   ./run.sh --img my.img   # другой образ диска
#   ./run.sh --rebuild      # пересобрать образ перед запуском
#   ./run.sh --help

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

# --- defaults ---
FULLSCREEN=1
USE_KVM=1
MEM_MB=256
DISK_IMG="build/fat.img"
REBUILD=0

usage() {
    printf 'Usage: %s [options]\n\n' "$(basename "$0")"
    printf 'Options:\n'
    printf '  --window        run in a window instead of fullscreen\n'
    printf '  --no-kvm        disable KVM acceleration\n'
    printf '  --mem   MB      RAM in MiB (default: %d)\n' "$MEM_MB"
    printf '  --img   PATH    disk image to boot (default: %s)\n' "$DISK_IMG"
    printf '  --rebuild       recreate the disk image before booting\n'
    printf '  --help          show this help\n'
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --window)   FULLSCREEN=0; shift ;;
        --no-kvm)   USE_KVM=0;    shift ;;
        --mem)      [[ -n "${2:-}" ]] || die "--mem requires a value"; MEM_MB="$2"; shift 2 ;;
        --img)      [[ -n "${2:-}" ]] || die "--img requires a value"; DISK_IMG="$2"; shift 2 ;;
        --rebuild)  REBUILD=1; shift ;;
        --help|-h)  usage ;;
        *) die "Unknown option: $1" ;;
    esac
done

[[ "$MEM_MB" =~ ^[0-9]+$ && "$MEM_MB" -ge 64 ]] \
    || die "Memory must be ≥ 64 MiB (got: $MEM_MB)"

printf '%s%sNexusOS Launcher%s\n\n' "$BOLD" "$CYAN" "$RESET"

# --- зависимости ---
command -v qemu-system-x86_64 &>/dev/null \
    || die "qemu-system-x86_64 not found  (sudo apt install qemu-system-x86)"

# --- OVMF: ищем несколько стандартных путей ---
OVMF_CODE=""
for p in \
    /usr/share/OVMF/OVMF_CODE_4M.fd \
    /usr/share/OVMF/OVMF_CODE.fd \
    /usr/share/edk2/ovmf/OVMF_CODE.fd \
    /usr/share/edk2-ovmf/OVMF_CODE.fd \
    /usr/share/qemu/OVMF.fd
do
    [[ -f "$p" ]] && { OVMF_CODE="$p"; break; }
done
[[ -n "$OVMF_CODE" ]] || die "OVMF firmware not found  (sudo apt install ovmf)"

# OVMF_VARS: локальная копия (QEMU пишет туда переменные EFI)
OVMF_VARS="./OVMF_VARS.fd"
if [[ ! -f "$OVMF_VARS" ]]; then
    VARS_TEMPLATE=""
    for p in \
        /usr/share/OVMF/OVMF_VARS_4M.fd \
        /usr/share/OVMF/OVMF_VARS.fd \
        /usr/share/edk2/ovmf/OVMF_VARS.fd \
        /usr/share/edk2-ovmf/OVMF_VARS.fd
    do
        [[ -f "$p" ]] && { VARS_TEMPLATE="$p"; break; }
    done
    if [[ -n "$VARS_TEMPLATE" ]]; then
        cp "$VARS_TEMPLATE" "$OVMF_VARS"
        ok "Copied OVMF_VARS from $VARS_TEMPLATE"
    else
        warn "OVMF_VARS template not found — EFI variables won't persist"
        OVMF_VARS=""
    fi
fi

# --- образ диска ---
if [[ "$REBUILD" -eq 1 || ! -f "$DISK_IMG" ]]; then
    if [[ "$REBUILD" -eq 1 ]]; then
        info "Rebuilding disk image (--rebuild)"
    else
        warn "Disk image not found: $DISK_IMG — creating it now"
    fi
    bash create-img.sh --out "$DISK_IMG" \
        || die "create-img.sh failed"
fi

ok "Disk image: $DISK_IMG"
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

# --- display ---
DISPLAY_FLAGS=()
if [[ "$FULLSCREEN" -eq 1 ]]; then
    DISPLAY_FLAGS=(-full-screen -display gtk,gl=on)
    info "Display: fullscreen (press Ctrl+Alt+F to toggle)"
else
    DISPLAY_FLAGS=(-display gtk,gl=on)
    info "Display: windowed"
fi

# --- OVMF_VARS drive (опционально) ---
VARS_DRIVE=()
[[ -n "$OVMF_VARS" ]] && VARS_DRIVE=(-drive "if=pflash,format=raw,file=${OVMF_VARS}")

printf '\n%s%sStarting QEMU...%s\n\n' "$BOLD" "$CYAN" "$RESET"

exec qemu-system-x86_64 \
    -drive "if=pflash,format=raw,readonly=on,file=${OVMF_CODE}" \
    "${VARS_DRIVE[@]}" \
    -drive "format=raw,file=${DISK_IMG}" \
    -m "${MEM_MB}M" \
    -device qemu-xhci,id=xhci \
    -usb \
    -device usb-tablet \
    "${KVM_FLAGS[@]}" \
    "${DISPLAY_FLAGS[@]}"
