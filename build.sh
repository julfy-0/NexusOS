#!/usr/bin/env bash
set -u

# NexusOS 0.5.0 - Enstein build frontend
# Parallel build with four live progress bars.
# The bars are driven by real Make outputs; OS is the overall build progress.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

VERSION="0.5.0 - Enstein"
BAR_WIDTH=24
JOBS="${NEXUS_BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 2)}"
[[ "$JOBS" =~ ^[0-9]+$ ]] || JOBS=2
(( JOBS < 1 )) && JOBS=1

RESET='\033[0m'
BOLD='\033[1m'
CYAN='\033[36m'
GREEN='\033[32m'
RED='\033[31m'
GRAY='\033[90m'
YELLOW='\033[33m'

TTY=0
[[ -t 1 ]] && TTY=1
if (( ! TTY )); then
    RESET=''; BOLD=''; CYAN=''; GREEN=''; RED=''; GRAY=''; YELLOW=''
fi

# Objects are grouped by the subsystem they belong to.
KERNEL_OBJS=(
    build/entry.o build/kernel.o build/gdt.o build/gdt_asm.o build/idt.o build/isr.o
    build/paging.o build/kstate.o build/mem_kernel.o build/panic.o build/gui.o
    build/usermode.o build/target.o build/mount_table.o build/fs_registry.o
)
DRIVER_OBJS=(
    build/console.o build/pic.o build/cpu.o build/keyboard.o build/pit.o build/xhci.o
    build/mouse.o build/pci.o build/ahci.o build/fat32.o
)
BOOT_OBJS=(build/mem_efi.o build/boot.o build/BOOTX64.EFI)

APP_NAMES=(
    vfs neofetch sysinfo meminfo about whoami version date echo reverse len upper lower title
    calc sum hex dec isprime fib ls pwd cd mkdir rmdir touch rm cp mv cat less head tail grep
    diff find write append wc df du colors beep reboot halt shutdown uname man lspci uptime
    diskls diskcat hardware
)
APP_OBJS=(build/shell.o)
for app in "${APP_NAMES[@]}"; do APP_OBJS+=("build/${app}.o"); done

# Unique outputs that represent a completed NexusOS build.
ALL_OUTPUTS=("${KERNEL_OBJS[@]}" "${DRIVER_OBJS[@]}" "${BOOT_OBJS[@]}" "${APP_OBJS[@]}" build/kernel.elf iso/EFI/BOOT/BOOTX64.EFI iso/kernel.elf)
KERNEL_TOTAL=$(( ${#KERNEL_OBJS[@]} + 1 ))
DRIVER_TOTAL=${#DRIVER_OBJS[@]}
BOOT_TOTAL=${#BOOT_OBJS[@]}
OS_TOTAL=${#ALL_OUTPUTS[@]}

count_done() {
    local done=0 f
    for f in "$@"; do
        [[ -e "$f" ]] && ((done++))
    done
    printf '%s' "$done"
}

# Build without extra make chatter; make itself performs all compilation concurrently.
run_make() {
    make -s -j"$JOBS" iso >build_output.txt 2>&1
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
    kd="$(count_done "${KERNEL_OBJS[@]}" build/kernel.elf)"
    dd="$(count_done "${DRIVER_OBJS[@]}")"
    bd="$(count_done "${BOOT_OBJS[@]}")"
    od="$(count_done "${ALL_OUTPUTS[@]}")"

    if (( TTY )); then
        # Move to the beginning of the four-line dashboard.
        printf '\033[4A\r'
        print_bar "Kernel" "$kd" "$KERNEL_TOTAL" "BUILD"; printf '\n'
        print_bar "Drivers" "$dd" "$DRIVER_TOTAL" "BUILD"; printf '\n'
        print_bar "Bootloader" "$bd" "$BOOT_TOTAL" "BUILD"; printf '\n'
        print_bar "OS" "$od" "$OS_TOTAL" "BUILD"; printf '\n'
    else
        printf 'Kernel     %d/%d | Drivers %d/%d | Bootloader %d/%d | OS %d/%d\n' \
            "$kd" "$KERNEL_TOTAL" "$dd" "$DRIVER_TOTAL" \
            "$bd" "$BOOT_TOTAL" "$od" "$OS_TOTAL"
    fi
}


printf '%s%sNexusOS Build System%s\n' "$BOLD" "$CYAN" "$RESET"
printf '%sVersion: %s%s  |  Parallel jobs: %s%s\n\n' "$GRAY" "$VERSION" "$GRAY" "$JOBS" "$RESET"

make -s clean >/dev/null 2>&1 || {
    printf '%sBuild cleanup failed.%s\n' "$RED" "$RESET" >&2
    exit 1
}

# Reserve four dashboard lines before starting the real parallel build.
if (( TTY )); then
    print_bar "Kernel" 0 "$KERNEL_TOTAL" "BUILD"; printf '\n'
    print_bar "Drivers" 0 "$DRIVER_TOTAL" "BUILD"; printf '\n'
    print_bar "Bootloader" 0 "$BOOT_TOTAL" "BUILD"; printf '\n'
    print_bar "OS" 0 "$OS_TOTAL" "BUILD"; printf '\n'
fi

run_make &
MAKE_PID=$!

while kill -0 "$MAKE_PID" 2>/dev/null; do
    redraw
    sleep 0.08
done

wait "$MAKE_PID"
RESULT=$?

# One final refresh after make has stopped.
if (( RESULT == 0 )); then
    # Ensure the final display is actually 100% for every stage.
    if (( TTY )); then
        printf '\033[4A\r'
        print_bar "Kernel" "$KERNEL_TOTAL" "$KERNEL_TOTAL" "OK"; printf '\n'
        print_bar "Drivers" "$DRIVER_TOTAL" "$DRIVER_TOTAL" "OK"; printf '\n'
        print_bar "Bootloader" "$BOOT_TOTAL" "$BOOT_TOTAL" "OK"; printf '\n'
        print_bar "OS" "$OS_TOTAL" "$OS_TOTAL" "OK"; printf '\n'
    else
        printf '\n'
        printf 'Kernel     100%%  OK | Drivers 100%%  OK | Bootloader 100%%  OK | OS 100%%  OK\n'
    fi
    printf '\n%s%sBuild complete!%s\n' "$BOLD" "$GREEN" "$RESET"
    printf '  %sJobs:%s     %s\n' "$GRAY" "$RESET" "$JOBS"
    printf '  %sEFI:%s      build/BOOTX64.EFI\n' "$GRAY" "$RESET"
    printf '  %sKernel:%s   build/kernel.elf\n' "$GRAY" "$RESET"
    printf '  %sESP:%s      iso/\n' "$GRAY" "$RESET"
else
    printf '\n%s%sBuild failed.%s\n' "$RED" "$BOLD" "$RESET" >&2
    printf '%sSee build_output.txt for the compiler output.%s\n' "$YELLOW" "$RESET" >&2
    cat build_output.txt >&2
    exit "$RESULT"
fi
