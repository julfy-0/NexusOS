#!/usr/bin/env bash
# NexusOS Build System
# Build frontend for the existing freestanding x86_64 kernel + UEFI loader.
# This script does not compile sources itself; GNU Make remains the source of truth.

set -u -o pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

VERSION="0.6.0"
ISO_IMAGE="build/NexusOS-${VERSION}.iso"
BAR_WIDTH=26
LOG_FILE="build_output.txt"
NO_CLEAN=0
IMAGE_ONLY=0
OUT_IMG="${NEXUS_IMAGE:-NexusOS.img}"
OUT_ISO="${NEXUS_ISO:-NexusOS.iso}"
IMAGE_SIZE_SPEC=""
IMAGE_CREATE=1
IMAGE_LAUNCH=0
ISO_CREATE=1
MEM_MB=256
USE_KVM=1
FULLSCREEN=1
AUTO_PROMPT=1

# -----------------------------------------------------------------------------
# Options / jobs
# -----------------------------------------------------------------------------

cpu_count() {
    local n=""
    n="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
    if [[ ! "$n" =~ ^[1-9][0-9]*$ ]]; then
        n="$(nproc 2>/dev/null || true)"
    fi
    if [[ ! "$n" =~ ^[1-9][0-9]*$ ]]; then
        n=1
    fi
    printf '%s\n' "$n"
}

JOBS="${NEXUS_BUILD_JOBS:-$(cpu_count)}"
[[ "$JOBS" =~ ^[1-9][0-9]*$ ]] || JOBS="$(cpu_count)"

usage() {
    cat <<USAGE
NexusOS Build System

Usage: ./build.sh [options]

Options:
  --no-clean        keep existing build/ and iso/ outputs
  --image-only      skip compilation and enter the media/launch phase directly
  --run             launch NexusOS after the build/media phase
  --no-launch       never launch QEMU (non-interactive)
  --no-prompt       do not ask interactive media/launch questions
  --no-img          do not create NexusOS.img
  --no-iso          do not create NexusOS.iso
  --size SIZE       total disk image size, minimum 192MB, e.g. 192M, 2G, 1T
  --out PATH        output GPT image (default: $OUT_IMG)
  --iso-out PATH    output bootable ISO (default: $OUT_ISO)
  -h, --help        show this help

Environment:
  NEXUS_BUILD_JOBS=N  override the automatic CPU-core job count
  NEXUS_IMAGE=PATH    default disk image path
  NEXUS_ISO=PATH      default bootable ISO path
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-clean) NO_CLEAN=1; shift ;;
        --image-only) IMAGE_ONLY=1; NO_CLEAN=1; shift ;;
        --size)
            [[ $# -ge 2 ]] || { printf '%s\n' '--size requires a value' >&2; exit 2; }
            IMAGE_SIZE_SPEC="$2"
            shift 2
            ;;
        --out)
            [[ $# -ge 2 ]] || { printf '%s\n' '--out requires a path' >&2; exit 2; }
            OUT_IMG="$2"
            shift 2
            ;;
        --iso-out)
            [[ $# -ge 2 ]] || { printf '%s\n' '--iso-out requires a path' >&2; exit 2; }
            OUT_ISO="$2"
            shift 2
            ;;
        --run) IMAGE_LAUNCH=1; AUTO_PROMPT=0; shift ;;
        --no-launch) IMAGE_LAUNCH=0; AUTO_PROMPT=0; shift ;;
        --no-prompt) AUTO_PROMPT=0; shift ;;
        --no-img) IMAGE_CREATE=0; shift ;;
        --no-iso) ISO_CREATE=0; shift ;;
        --mem)
            [[ $# -ge 2 ]] || { printf '%s\n' '--mem requires a value' >&2; exit 2; }
            MEM_MB="$2"
            shift 2
            ;;
        -h|--help) usage; exit 0 ;;
        *) printf 'Unknown option: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
done

# -----------------------------------------------------------------------------
# Terminal UI
# -----------------------------------------------------------------------------

if [[ -t 1 ]]; then
    RESET=$'\033[0m'
    BOLD=$'\033[1m'
    CYAN=$'\033[36m'
    GREEN=$'\033[32m'
    RED=$'\033[31m'
    YELLOW=$'\033[33m'
    GRAY=$'\033[90m'
    TTY=1
else
    RESET=''; BOLD=''; CYAN=''; GREEN=''; RED=''; YELLOW=''; GRAY=''
    TTY=0
fi

printf '%s%sNexusOS Build System%s\n' "$BOLD" "$CYAN" "$RESET"
printf '%sVersion: %s%s  |  Parallel jobs: %s%s\n\n' "$GRAY" "$VERSION" "$GRAY" "$JOBS" "$RESET"

# -----------------------------------------------------------------------------
# Build artifact manifests
# -----------------------------------------------------------------------------

map_obj() {
    local src="$1"
    case "$src" in
        *.c) printf 'build/%s.o\n' "${src%.c}" ;;
        *.S) printf 'build/%s.o\n' "${src%.S}" ;;
        *) return 1 ;;
    esac
}

KERNEL_ARTIFACTS=()
while IFS= read -r src; do
    KERNEL_ARTIFACTS+=("$(map_obj "$src")")
done < <(find kernel drivers fs lib gui shell platform -type f \( -name '*.c' -o -name '*.S' \) \
    ! -path 'kernel/bootmode/*' -print | sort)
KERNEL_ARTIFACTS+=(
    "build/assets/wallpapers/nexus_default.o"
    "build/kernel.elf"
)

DRIVER_ARTIFACTS=()
while IFS= read -r src; do
    DRIVER_ARTIFACTS+=("$(map_obj "$src")")
done < <(find drivers -type f -name '*.c' -print | sort)

BOOT_ARTIFACTS=(
    "build/boot/uefi/src/boot.o"
    "build/boot/uefi/mem.o"
    "build/BOOTX64.EFI"
)

SYSTEM_ARTIFACTS=(
    "iso/EFI/BOOT/BOOTX64.EFI"
    "iso/kernel.elf"
    "build/NexusOS-0.6.0.iso"
)

# OS = unique set of all real build/staging artifacts. No artificial counters.
declare -A OS_SEEN=()
OS_ARTIFACTS=()
for artifact in "${KERNEL_ARTIFACTS[@]}" "${BOOT_ARTIFACTS[@]}" "${SYSTEM_ARTIFACTS[@]}"; do
    if [[ -z "${OS_SEEN[$artifact]+x}" ]]; then
        OS_SEEN["$artifact"]=1
        OS_ARTIFACTS+=("$artifact")
    fi
done

count_existing() {
    local count=0 file
    for file in "$@"; do
        [[ -s "$file" ]] && ((count += 1))
    done
    printf '%d\n' "$count"
}

bar_line() {
    local name="$1" done="$2" total="$3" state="$4"
    local pct=0 filled=0 empty=0 bar line_color line_state

    if (( total > 0 )); then
        pct=$((done * 100 / total))
    fi
    (( pct > 100 )) && pct=100
    filled=$((pct * BAR_WIDTH / 100))
    empty=$((BAR_WIDTH - filled))
    bar="$(printf '%*s' "$filled" '' | tr ' ' '#')$(printf '%*s' "$empty" '' | tr ' ' '.')"

    # A component that has reached 100% is finished. Keep the whole row
    # green and explicitly mark it as BUILDED, while unfinished rows keep
    # the normal BUILD state.
    line_color=""
    line_state="$state"
    if (( pct == 100 )); then
        line_color="$GREEN"
        line_state="BUILDED"
    fi

    printf '%s%-11s [%s] %3d%%  %s%s' "$line_color" "$name" "$bar" "$pct" "$line_state" "$RESET"
}

render() {
    local kd dd bd sd od
    kd="$(count_existing "${KERNEL_ARTIFACTS[@]}")"
    dd="$(count_existing "${DRIVER_ARTIFACTS[@]}")"
    bd="$(count_existing "${BOOT_ARTIFACTS[@]}")"
    sd="$(count_existing "${SYSTEM_ARTIFACTS[@]}")"
    od="$(count_existing "${OS_ARTIFACTS[@]}")"

    if (( TTY )); then
        printf '\033[5A\r'
        bar_line "Kernel" "$kd" "${#KERNEL_ARTIFACTS[@]}" "BUILD"; printf '\n'
        bar_line "Drivers" "$dd" "${#DRIVER_ARTIFACTS[@]}" "BUILD"; printf '\n'
        bar_line "Bootloader" "$bd" "${#BOOT_ARTIFACTS[@]}" "BUILD"; printf '\n'
        bar_line "System" "$sd" "${#SYSTEM_ARTIFACTS[@]}" "BUILD"; printf '\n'
        bar_line "OS" "$od" "${#OS_ARTIFACTS[@]}" "BUILD"; printf '\n'
    else
        printf 'Kernel %d/%d | Drivers %d/%d | Bootloader %d/%d | System %d/%d | OS %d/%d\n' \
            "$kd" "${#KERNEL_ARTIFACTS[@]}" "$dd" "${#DRIVER_ARTIFACTS[@]}" \
            "$bd" "${#BOOT_ARTIFACTS[@]}" "$sd" "${#SYSTEM_ARTIFACTS[@]}" \
            "$od" "${#OS_ARTIFACTS[@]}"
    fi
}

render_final() {
    local state="$1"
    local kd dd bd sd od
    kd="$(count_existing "${KERNEL_ARTIFACTS[@]}")"
    dd="$(count_existing "${DRIVER_ARTIFACTS[@]}")"
    bd="$(count_existing "${BOOT_ARTIFACTS[@]}")"
    sd="$(count_existing "${SYSTEM_ARTIFACTS[@]}")"
    od="$(count_existing "${OS_ARTIFACTS[@]}")"

    if (( TTY )); then
        printf '\033[5A\r'
        bar_line "Kernel" "$kd" "${#KERNEL_ARTIFACTS[@]}" "$state"; printf '\n'
        bar_line "Drivers" "$dd" "${#DRIVER_ARTIFACTS[@]}" "$state"; printf '\n'
        bar_line "Bootloader" "$bd" "${#BOOT_ARTIFACTS[@]}" "$state"; printf '\n'
        bar_line "System" "$sd" "${#SYSTEM_ARTIFACTS[@]}" "$state"; printf '\n'
        bar_line "OS" "$od" "${#OS_ARTIFACTS[@]}" "$state"; printf '\n'
    fi
}

# -----------------------------------------------------------------------------
# Unified image / ISO / QEMU helpers
# -----------------------------------------------------------------------------

# Media and launcher variables are initialized before CLI option parsing.

parse_size_spec() {
    local spec="$1"
    python3 - "$spec" <<'PY_SIZE'
import sys
s=sys.argv[1].strip().upper()
units={'M':1024**2,'G':1024**3,'T':1024**4,'MB':1024**2,'GB':1024**3,'TB':1024**4}
if s.isdigit():
    v=int(s)*1024**2
else:
    unit=next((u for u in ('TB','GB','MB','T','G','M') if s.endswith(u)),None)
    if unit is None or not s[:-len(unit)].isdigit():
        raise SystemExit(2)
    v=int(s[:-len(unit)])*units[unit]
if v < 192*1024*1024 or v % 512:
    raise SystemExit(2)
print(v)
PY_SIZE
}

choose_yes_no() {
    local prompt="$1" default="$2" reply
    if (( ! TTY )); then [[ "$default" == "Y" ]]
        return
    fi
    read -r -p "$prompt [$default]: " reply || reply=""
    reply="${reply:-$default}"
    case "$reply" in
        Y|y|yes|YES) return 0 ;;
        N|n|no|NO) return 1 ;;
        *) [[ "$default" == "Y" ]] ;;
    esac
}

prompt_build_actions() {
    (( AUTO_PROMPT )) || return 0
    if choose_yes_no "Want to create NexusOS.img?" "Y"; then
        IMAGE_CREATE=1
        if [[ -z "$IMAGE_SIZE_SPEC" && -t 0 ]]; then
            local size_reply
            read -r -p "Image size [192MB] (MB/GB/TB, e.g. 192MB, 4GB, 1TB): " size_reply || size_reply=""
            IMAGE_SIZE_SPEC="${size_reply:-192MB}"
        fi
        if [[ -z "$IMAGE_SIZE_SPEC" ]]; then IMAGE_SIZE_SPEC="192MB"; fi
    else
        IMAGE_CREATE=0
    fi

    if choose_yes_no "Want to create NexusOS.iso?" "Y"; then
        ISO_CREATE=1
    else
        ISO_CREATE=0
    fi

    if choose_yes_no "Want to launch NexusOS?" "N"; then
        IMAGE_LAUNCH=1
    else
        IMAGE_LAUNCH=0
    fi
}

create_image_and_iso() {
    [[ -s build/BOOTX64.EFI ]] || { printf '%s%sERROR:%s build/BOOTX64.EFI is missing\n' "$BOLD" "$RED" "$RESET" >&2; return 1; }
    [[ -s build/kernel.elf ]] || { printf '%s%sERROR:%s build/kernel.elf is missing\n' "$BOLD" "$RED" "$RESET" >&2; return 1; }
    [[ -f iso/EFI/BOOT/BOOTX64.EFI ]] || { printf '%s%sERROR:%s iso/EFI/BOOT/BOOTX64.EFI is missing\n' "$BOLD" "$RED" "$RESET" >&2; return 1; }
    [[ -f iso/kernel.elf ]] || { printf '%s%sERROR:%s iso/kernel.elf is missing\n' "$BOLD" "$RED" "$RESET" >&2; return 1; }
    command -v python3 >/dev/null 2>&1 || { printf '%s%sERROR:%s python3 is required for image creation\n' "$BOLD" "$RED" "$RESET" >&2; return 1; }

    if (( IMAGE_CREATE )); then
        if ! parse_size_spec "$IMAGE_SIZE_SPEC" >/dev/null; then
            printf '%s%sERROR:%s invalid image size: %s (minimum 192MB; use MB/GB/TB such as 192MB, 4GB, 1TB)\n' "$BOLD" "$RED" "$RESET" "$IMAGE_SIZE_SPEC" >&2
            return 2
        fi
        mkdir -p "$(dirname "$OUT_IMG")"
        export NEXUS_ROOT_DIR="$ROOT_DIR"
        export NEXUS_OUT_IMG="$OUT_IMG"
        export NEXUS_IMAGE_SIZE_SPEC="$IMAGE_SIZE_SPEC"
        printf '%s=>%s Creating GPT/FAT32 image: %s (%s)\n' "$CYAN" "$RESET" "$OUT_IMG" "$IMAGE_SIZE_SPEC"
        python3 tools/create_image.py
        printf '%s%sImage created:%s %s\n' "$GREEN" "$BOLD" "$RESET" "$OUT_IMG"
    fi

    if (( ISO_CREATE )); then
        mkdir -p "$(dirname "$OUT_ISO")"
        printf '%s=>%s Creating bootable ISO: %s\n' "$CYAN" "$RESET" "$OUT_ISO"
        python3 tools/create_iso.py --bootloader build/BOOTX64.EFI --kernel build/kernel.elf --out "$OUT_ISO"
        [[ -s "$OUT_ISO" ]] || { printf '%s%sERROR:%s ISO was not created\n' "$BOLD" "$RED" "$RESET" >&2; return 1; }
        printf '%s%sISO created:%s %s\n' "$GREEN" "$BOLD" "$RESET" "$OUT_ISO"
    fi
}

launch_nexus() {
    command -v qemu-system-x86_64 >/dev/null 2>&1 || { printf '%s%sERROR:%s qemu-system-x86_64 not found\n' "$BOLD" "$RED" "$RESET" >&2; return 1; }
    local ovmf_code ovmf_vars vars_template
    find_first_file() {
        local candidate
        for candidate in "$@"; do [[ -f "$candidate" ]] && { printf '%s\n' "$candidate"; return 0; }; done
        return 1
    }
    ovmf_code="${OVMF_CODE:-}"
    if [[ -z "$ovmf_code" ]]; then
        ovmf_code="$(find_first_file /usr/share/OVMF/OVMF_CODE_4M.fd /usr/share/OVMF/OVMF_CODE.fd /usr/share/edk2/ovmf/OVMF_CODE.fd /usr/share/qemu/OVMF.fd "$ROOT_DIR/OVMF_CODE.fd" 2>/dev/null || true)"
    fi
    [[ -n "$ovmf_code" && -r "$ovmf_code" ]] || { printf '%s%sERROR:%s OVMF_CODE not found\n' "$BOLD" "$RED" "$RESET" >&2; return 1; }
    ovmf_vars="${OVMF_VARS:-$ROOT_DIR/OVMF_VARS.fd}"
    if [[ ! -f "$ovmf_vars" ]]; then
        vars_template="$(find_first_file /usr/share/OVMF/OVMF_VARS_4M.fd /usr/share/OVMF/OVMF_VARS.fd /usr/share/edk2/ovmf/OVMF_VARS.fd 2>/dev/null || true)"
        [[ -n "$vars_template" ]] || { printf '%s%sERROR:%s OVMF_VARS template not found\n' "$BOLD" "$RED" "$RESET" >&2; return 1; }
        cp "$vars_template" "$ovmf_vars"
    fi
    if [[ ! -s "$OUT_IMG" && -s "$OUT_ISO" ]]; then
        qemu-system-x86_64 -drive "if=pflash,format=raw,readonly=on,file=${ovmf_code}" -drive "if=pflash,format=raw,file=${ovmf_vars}" -cdrom "$OUT_ISO" -m "${MEM_MB}M" -device qemu-xhci,id=xhci -usb -device usb-tablet -cpu qemu64 -display gtk,gl=on
    else
        [[ -s "$OUT_IMG" ]] || { printf '%s%sERROR:%s NexusOS.img not found and no ISO available\n' "$BOLD" "$RED" "$RESET" >&2; return 1; }
        local accel=(-cpu qemu64)
        if (( USE_KVM )) && [[ -r /dev/kvm ]]; then accel=(-enable-kvm -cpu host); fi
        qemu-system-x86_64 -drive "if=pflash,format=raw,readonly=on,file=${ovmf_code}" -drive "if=pflash,format=raw,file=${ovmf_vars}" -drive "format=raw,file=${OUT_IMG}" -m "${MEM_MB}M" -device qemu-xhci,id=xhci -usb -device usb-tablet "${accel[@]}" -display gtk,gl=on
    fi
}

# -----------------------------------------------------------------------------
# Preconditions / clean
# -----------------------------------------------------------------------------

command -v make >/dev/null 2>&1 || { printf '%smake not found%s\n' "$RED" "$RESET" >&2; exit 1; }

if (( ! NO_CLEAN )); then
    if ! make --no-print-directory clean >/dev/null 2>&1; then
        printf '%sBuild cleanup failed.%s\n' "$RED" "$RESET" >&2
        exit 1
    fi
    rm -f "$LOG_FILE"
fi

if (( TTY )); then
    bar_line "Kernel" 0 "${#KERNEL_ARTIFACTS[@]}" "BUILD"; printf '\n'
    bar_line "Drivers" 0 "${#DRIVER_ARTIFACTS[@]}" "BUILD"; printf '\n'
    bar_line "Bootloader" 0 "${#BOOT_ARTIFACTS[@]}" "BUILD"; printf '\n'
    bar_line "System" 0 "${#SYSTEM_ARTIFACTS[@]}" "BUILD"; printf '\n'
    bar_line "OS" 0 "${#OS_ARTIFACTS[@]}" "BUILD"; printf '\n'
fi

# -----------------------------------------------------------------------------
# Real build
# -----------------------------------------------------------------------------

# The Makefile target "system" is the existing ISO/EFI staging integration:
# bootloader + kernel + iso/EFI/BOOT/BOOTX64.EFI + iso/kernel.elf.
make --no-print-directory -j"$JOBS" iso >"$LOG_FILE" 2>&1 &
MAKE_PID=$!

while kill -0 "$MAKE_PID" 2>/dev/null; do
    render
    sleep 0.08
done

if wait "$MAKE_PID"; then
    RESULT=0
else
    RESULT=$?
fi

if (( RESULT != 0 )); then
    render_final "FAIL"
    printf '\n%s%sBuild failed.%s\n' "$RED" "$BOLD" "$RESET" >&2
    printf '%sCompiler output: %s%s\n' "$YELLOW" "$LOG_FILE" "$RESET" >&2
    cat "$LOG_FILE" >&2
    exit "$RESULT"
fi

render_final "OK"
printf '\n%s%sBuild complete!%s\n' "$BOLD" "$GREEN" "$RESET"
printf '  %sJobs:%s       %s\n' "$GRAY" "$RESET" "$JOBS"
printf '  %sBootloader:%s build/BOOTX64.EFI\n' "$GRAY" "$RESET"
printf '  %sKernel:%s     build/kernel.elf\n' "$GRAY" "$RESET"
printf '  %sSystem:%s     iso/\n' "$GRAY" "$RESET"
printf '  %sISO staging:%s iso/\n' "$GRAY" "$RESET"
printf '  %sLog:%s        %s\n' "$GRAY" "$RESET" "$LOG_FILE"


prompt_build_actions
if ! create_image_and_iso; then
    printf '%s%sMedia creation failed.%s\n' "$RED" "$BOLD" "$RESET" >&2
    exit 1
fi
printf '%s%sBuild + media phase complete!%s\n' "$BOLD" "$GREEN" "$RESET"
if (( IMAGE_CREATE )); then printf '  %sDisk image:%s %s\n' "$GRAY" "$RESET" "$OUT_IMG"; fi
if (( ISO_CREATE )); then printf '  %sBootable ISO:%s %s\n' "$GRAY" "$RESET" "$OUT_ISO"; fi
if (( IMAGE_LAUNCH )); then
    printf '\n%s=>%s Launching NexusOS...%s\n' "$CYAN" "$RESET" "$RESET"
    launch_nexus
fi
