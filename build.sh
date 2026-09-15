#!/usr/bin/env bash
# NexusOS Build System
# Build frontend for the existing freestanding x86_64 kernel + UEFI loader.
# This script does not compile sources itself; GNU Make remains the source of truth.

set -u -o pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

VERSION="0.5.18"
ISO_IMAGE="build/NexusOS-${VERSION}.iso"
BAR_WIDTH=26
LOG_FILE="build_output.txt"
NO_CLEAN=0

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

Usage: ./build.sh [--no-clean]

Options:
  --no-clean       keep existing build/ and iso/ outputs
  -h, --help       show this help

Environment:
  NEXUS_BUILD_JOBS=N  override the automatic CPU-core job count
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-clean) NO_CLEAN=1; shift ;;
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
    "build/NexusOS-0.5.18.iso"
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
printf '  %sISO:%s        %s\n' "$GRAY" "$RESET" "$ISO_IMAGE"
printf '  %sLog:%s        %s\n' "$GRAY" "$RESET" "$LOG_FILE"
