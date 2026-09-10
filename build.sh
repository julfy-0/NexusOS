#!/usr/bin/env bash
set -u

# NexusOS 0.5.1 — parallel build frontend
# The Makefile remains the source of truth; this script only provides the
# readable progress dashboard and job-count control.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

VERSION="0.5.1"
BAR_WIDTH=24
JOBS="${NEXUS_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 2)}"
[[ "$JOBS" =~ ^[0-9]+$ ]] || JOBS=2
(( JOBS < 1 )) && JOBS=1

NO_CLEAN=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-clean) NO_CLEAN=1; shift ;;
        --help|-h)
            printf 'Usage: %s [--no-clean]\n' "$(basename "$0")"
            printf '  --no-clean   keep existing build/ and iso/ artifacts\n'
            printf '  NEXUS_BUILD_JOBS=8 %s\n' "$(basename "$0")"
            exit 0
            ;;
        *) printf 'Unknown option: %s\n' "$1" >&2; exit 2 ;;
    esac
done

RESET='\033[0m'; BOLD='\033[1m'; CYAN='\033[36m'
GREEN='\033[32m'; RED='\033[31m'; GRAY='\033[90m'; YELLOW='\033[33m'
TTY=0
[[ -t 1 ]] && TTY=1
if (( ! TTY )); then
    RESET=''; BOLD=''; CYAN=''; GREEN=''; RED=''; GRAY=''; YELLOW=''
fi

# Expected outputs mirror the Makefile's generic source-to-object mapping.
map_obj() {
    local f="$1"
    case "$f" in
        *.c) f="${f%.c}.o" ;;
        *.S) f="${f%.S}.o" ;;
    esac
    printf 'build/%s' "$f"
}

KERNEL_OUTPUTS=()
while IFS= read -r f; do KERNEL_OUTPUTS+=("$(map_obj "$f")"); done \
    < <(find kernel fs lib gui shell platform -type f \( -name '*.c' -o -name '*.S' \) ! -path 'kernel/bootmode/*' | sort)
KERNEL_OUTPUTS+=("build/assets/wallpapers/nexus_default.o" "build/kernel.elf")

DRIVER_OUTPUTS=()
while IFS= read -r f; do DRIVER_OUTPUTS+=("$(map_obj "$f")"); done \
    < <(find drivers -type f -name '*.c' | sort)

BOOT_OUTPUTS=("build/boot/uefi/src/boot.o" "build/boot/uefi/mem.o" "build/BOOTX64.EFI")
ALL_OUTPUTS=("${KERNEL_OUTPUTS[@]}" "${BOOT_OUTPUTS[@]}" "iso/EFI/BOOT/BOOTX64.EFI" "iso/kernel.elf")

count_done() {
    local done=0 f
    for f in "$@"; do [[ -e "$f" ]] && ((done++)); done
    printf '%s' "$done"
}

print_bar() {
    local name="$1" done="$2" total="$3" state="${4:-BUILD}"
    local pct=0 filled=0 empty=0
    (( total > 0 )) && pct=$((done * 100 / total))
    (( pct > 100 )) && pct=100
    filled=$((pct * BAR_WIDTH / 100))
    empty=$((BAR_WIDTH - filled))
    local bar
    bar="$(printf '%*s' "$filled" '' | tr ' ' '#')$(printf '%*s' "$empty" '' | tr ' ' '.')"
    printf '%-11s [%s] %3d%%  %s' "$name" "$bar" "$pct" "$state"
}

redraw() {
    local kd dd bd od
    kd="$(count_done "${KERNEL_OUTPUTS[@]}")"
    dd="$(count_done "${DRIVER_OUTPUTS[@]}")"
    bd="$(count_done "${BOOT_OUTPUTS[@]}")"
    od="$(count_done "${ALL_OUTPUTS[@]}")"

    if (( TTY )); then
        printf '\033[4A\r'
        print_bar "Kernel" "$kd" "${#KERNEL_OUTPUTS[@]}" "BUILD"; printf '\n'
        print_bar "Drivers" "$dd" "${#DRIVER_OUTPUTS[@]}" "BUILD"; printf '\n'
        print_bar "Bootloader" "$bd" "${#BOOT_OUTPUTS[@]}" "BUILD"; printf '\n'
        print_bar "OS" "$od" "${#ALL_OUTPUTS[@]}" "BUILD"; printf '\n'
    else
        printf 'Kernel %d/%d | Drivers %d/%d | Bootloader %d/%d | OS %d/%d\n' \
            "$kd" "${#KERNEL_OUTPUTS[@]}" "$dd" "${#DRIVER_OUTPUTS[@]}" \
            "$bd" "${#BOOT_OUTPUTS[@]}" "$od" "${#ALL_OUTPUTS[@]}"
    fi
}

printf '%s%sNexusOS Build System%s\n' "$BOLD" "$CYAN" "$RESET"
printf '%sVersion: %s%s  |  Parallel jobs: %s%s\n\n' "$GRAY" "$VERSION" "$GRAY" "$JOBS" "$RESET"

if (( ! NO_CLEAN )); then
    make -s clean >/dev/null 2>&1 || {
        printf '%sBuild cleanup failed.%s\n' "$RED" "$RESET" >&2
        exit 1
    }
fi

if (( TTY )); then
    print_bar "Kernel" 0 "${#KERNEL_OUTPUTS[@]}" "BUILD"; printf '\n'
    print_bar "Drivers" 0 "${#DRIVER_OUTPUTS[@]}" "BUILD"; printf '\n'
    print_bar "Bootloader" 0 "${#BOOT_OUTPUTS[@]}" "BUILD"; printf '\n'
    print_bar "OS" 0 "${#ALL_OUTPUTS[@]}" "BUILD"; printf '\n'
fi

make -s -j"$JOBS" iso >build_output.txt 2>&1 &
MAKE_PID=$!

while kill -0 "$MAKE_PID" 2>/dev/null; do
    redraw
    sleep 0.08
done

wait "$MAKE_PID"
RESULT=$?

if (( RESULT == 0 )); then
    if (( TTY )); then
        printf '\033[4A\r'
        print_bar "Kernel" "${#KERNEL_OUTPUTS[@]}" "${#KERNEL_OUTPUTS[@]}" "OK"; printf '\n'
        print_bar "Drivers" "${#DRIVER_OUTPUTS[@]}" "${#DRIVER_OUTPUTS[@]}" "OK"; printf '\n'
        print_bar "Bootloader" "${#BOOT_OUTPUTS[@]}" "${#BOOT_OUTPUTS[@]}" "OK"; printf '\n'
        print_bar "OS" "${#ALL_OUTPUTS[@]}" "${#ALL_OUTPUTS[@]}" "OK"; printf '\n'
    else
        printf '\nKernel 100%% OK | Drivers 100%% OK | Bootloader 100%% OK | OS 100%% OK\n'
    fi
    printf '\n%s%sBuild complete!%s\n' "$BOLD" "$GREEN" "$RESET"
    printf '  %sJobs:%s     %s\n' "$GRAY" "$RESET" "$JOBS"
    printf '  %sEFI:%s      build/BOOTX64.EFI\n' "$GRAY" "$RESET"
    printf '  %sKernel:%s   build/kernel.elf\n' "$GRAY" "$RESET"
    printf '  %sESP:%s      iso/\n' "$GRAY" "$RESET"
else
    printf '\n%s%sBuild failed.%s\n' "$RED" "$BOLD" "$RESET" >&2
    printf '%sSee build_output.txt for compiler output.%s\n' "$YELLOW" "$RESET" >&2
    cat build_output.txt >&2
    exit "$RESULT"
fi
