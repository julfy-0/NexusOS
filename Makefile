# NexusOS 0.5.2 — unified freestanding build
#
# The source tree is organized by subsystem. Object files mirror the source
# paths under build/, which prevents filename collisions as the project grows.
#
# Requirements on x86_64 Linux:
#   gcc, binutils
#   make iso: no additional tools
#   make run: qemu-system-x86_64, OVMF, dosfstools, mtools

CC      := gcc
LD      := ld
OBJCOPY := objcopy

BUILD := build
ISODIR := iso

# Every kernel-side module gets access to its own directory plus the shared
# public headers. This keeps source includes short and avoids fragile
# ../../ paths when modules move.
MODULE_DIRS := $(shell find kernel drivers fs lib gui shell system platform -type d 2>/dev/null | sort)
INCLUDES := -Iinclude/nexus $(addprefix -I,$(MODULE_DIRS)) -Iassets/fonts

# --- UEFI bootloader: freestanding PE32+, MS x64 ABI ---
EFI_INCLUDES := -Iinclude/nexus -Iboot/uefi/include -Iboot/uefi/src -Iboot/uefi/assets
CFLAGS_EFI := -ffreestanding -fno-stack-protector -fno-stack-check \
              -fshort-wchar -mno-red-zone -fpic -fno-ident \
              -Wall -Wextra -O2 $(EFI_INCLUDES) -c
LDFLAGS_EFI := -m i386pep -nostdlib -shared -Bsymbolic -e efi_main --subsystem 10

# --- Kernel: freestanding ELF64, System V ABI ---
CFLAGS_KERNEL := -ffreestanding -fno-stack-protector -fno-stack-check \
                 -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -mgeneral-regs-only \
                 -fno-pic -fno-pie -fno-ident \
                 -Wall -Wextra -O2 $(INCLUDES) -c
LDFLAGS_KERNEL := -nostdlib -static -T kernel/arch/x86_64/linker.ld

KERNEL_C_SRCS := $(shell find kernel drivers fs lib gui shell system platform -type f -name '*.c' ! -path 'kernel/bootmode/*' 2>/dev/null | sort)
KERNEL_S_SRCS := $(shell find kernel/arch/x86_64 -type f -name '*.S' 2>/dev/null | sort)

KERNEL_C_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(KERNEL_C_SRCS))
KERNEL_S_OBJS := $(patsubst %.S,$(BUILD)/%.o,$(KERNEL_S_SRCS))
WALLPAPER_OBJ := $(BUILD)/assets/wallpapers/nexus_default.o

# Keep architecture entry stubs first in the linker input, then C modules.
KERNEL_OBJS := $(KERNEL_S_OBJS) $(KERNEL_C_OBJS) $(WALLPAPER_OBJ)

BOOT_OBJS := $(BUILD)/boot/uefi/src/boot.o $(BUILD)/boot/uefi/mem.o

.PHONY: all clean bootloader kernel iso run check check-boot

all: bootloader kernel

$(BUILD):
	mkdir -p $(BUILD)

# ---------------------------------------------------------------------------
# UEFI bootloader
# ---------------------------------------------------------------------------

$(BUILD)/boot/uefi/src/boot.o: boot/uefi/src/boot.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_EFI) $< -o $@

# lib/memory/mem.c is deliberately compiled twice: once for EFI's ABI and
# once for the kernel's freestanding environment.
$(BUILD)/boot/uefi/mem.o: lib/memory/mem.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_EFI) $< -o $@

bootloader: $(BOOT_OBJS)
	$(LD) $(LDFLAGS_EFI) -o $(BUILD)/BOOTX64.EFI $(BOOT_OBJS)
	@echo "==> UEFI bootloader: $(BUILD)/BOOTX64.EFI"

# ---------------------------------------------------------------------------
# Generic kernel-side compilation
# ---------------------------------------------------------------------------

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_KERNEL) $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) -ffreestanding -fno-ident -mno-red-zone -c $< -o $@

$(WALLPAPER_OBJ): assets/wallpapers/nexus_default.rgb565 | $(BUILD)
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
		--rename-section .data=.rodata,alloc,load,readonly,data,contents \
		$< $@

kernel: $(KERNEL_OBJS)
	$(LD) $(LDFLAGS_KERNEL) -o $(BUILD)/kernel.elf $(KERNEL_OBJS)
	@echo "==> Kernel: $(BUILD)/kernel.elf"

# ---------------------------------------------------------------------------
# EFI System Partition staging
# ---------------------------------------------------------------------------

iso: bootloader kernel
	mkdir -p $(ISODIR)/EFI/BOOT
	cp $(BUILD)/BOOTX64.EFI $(ISODIR)/EFI/BOOT/BOOTX64.EFI
	cp $(BUILD)/kernel.elf $(ISODIR)/kernel.elf
	@echo "==> ESP staged in $(ISODIR)/"

# Keep make run compatible with the existing QEMU frontend.
run: iso
	./run.sh

# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------

check:
	@set -e; \
	for f in $$(find kernel drivers fs lib gui shell system platform -type f -name '*.c' ! -path 'kernel/bootmode/*' | sort); do \
		$(CC) $(CFLAGS_KERNEL) -fsyntax-only "$$f"; \
	done
	@echo "==> Kernel-side C syntax: OK"
	@$(MAKE) --no-print-directory check-boot

check-boot:
	@set -e; \
	for f in $$(find boot/uefi/src -type f -name '*.c' | sort); do \
		$(CC) $(CFLAGS_EFI) -fsyntax-only "$$f"; \
	done
	@echo "==> UEFI C syntax: OK"

clean:
	rm -rf $(BUILD) $(ISODIR)
