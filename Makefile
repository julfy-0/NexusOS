# NexusOS 0.6.2 — Enstein
#
# This Makefile is the single source of truth for compilation and linking.
# build.sh is only a progress/UX frontend around the real targets below.

SHELL := /bin/bash
.DEFAULT_GOAL := all

CC      ?= gcc
LD      ?= ld
OBJCOPY ?= objcopy

BUILD  := build
ISODIR := iso
NEXUS_VERSION ?= 0.6.5
ISO_IMAGE := $(BUILD)/NexusOS-$(NEXUS_VERSION).iso

# -----------------------------------------------------------------------------
# Include paths
# -----------------------------------------------------------------------------

MODULE_DIRS := $(shell find kernel drivers fs lib gui shell platform system -type d 2>/dev/null | sort)
INCLUDES := -Iinclude/nexus $(addprefix -I,$(MODULE_DIRS)) -Iassets/fonts

# UEFI bootloader: PE32+ x86-64, Microsoft x64 ABI.
EFI_INCLUDES := -Iinclude/nexus -Iboot/uefi/include -Iboot/uefi/src -Iboot/uefi/assets
CFLAGS_EFI := -ffreestanding -fno-stack-protector -fno-stack-check \
              -fshort-wchar -mno-red-zone -fpic -fno-ident \
              -Wall -Wextra -O2 $(EFI_INCLUDES)
LDFLAGS_EFI := -m i386pep -nostdlib -shared -Bsymbolic -e efi_main --subsystem 10

# Kernel: freestanding ELF64, System V ABI.
CFLAGS_KERNEL := -ffreestanding -fno-stack-protector -fno-stack-check \
                 -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -mgeneral-regs-only \
                 -fPIE -fno-ident \
                 -Wall -Wextra -O2 $(INCLUDES)
CFLAGS_ASM := -ffreestanding -fno-ident -mno-red-zone
LDFLAGS_KERNEL := -nostdlib -shared -Bsymbolic -T kernel/arch/x86_64/linker.ld

# -----------------------------------------------------------------------------
# Source/object manifests
# -----------------------------------------------------------------------------

KERNEL_C_SRCS := $(shell find kernel drivers fs lib gui shell platform system -type f -name '*.c' \
                  ! -path 'kernel/bootmode/*' ! -path 'system/core/init.c' ! -path 'system/core/system.c' 2>/dev/null | sort)
KERNEL_S_SRCS := $(shell find kernel/arch/x86_64 -type f -name '*.S' 2>/dev/null | sort)
DRIVER_C_SRCS := $(shell find drivers -type f -name '*.c' 2>/dev/null | sort)

KERNEL_C_OBJS := $(patsubst %.c,$(BUILD)/%.o,$(KERNEL_C_SRCS))
KERNEL_S_OBJS := $(patsubst %.S,$(BUILD)/%.o,$(KERNEL_S_SRCS))
DRIVER_OBJS   := $(patsubst %.c,$(BUILD)/%.o,$(DRIVER_C_SRCS))

WALLPAPER_OBJ := $(BUILD)/assets/wallpapers/nexus_default.o
FONT_REGULAR_OBJ := $(BUILD)/assets/fonts/Roboto-Regular.o
FONT_BOLD_OBJ := $(BUILD)/assets/fonts/Roboto-Bold.o
KERNEL_OBJS   := $(KERNEL_S_OBJS) $(KERNEL_C_OBJS) $(WALLPAPER_OBJ) $(FONT_REGULAR_OBJ) $(FONT_BOLD_OBJ)

# lib/memory/mem.c is intentionally compiled twice: once for UEFI and once
# for the kernel, producing different objects in different output paths.
BOOT_OBJS := \
    $(BUILD)/boot/uefi/src/boot.o \
    $(BUILD)/boot/uefi/mem.o

SYSTEM_OUTPUTS := \
    $(ISODIR)/EFI/BOOT/BOOTX64.EFI \
    $(ISODIR)/kernel.elf

.PHONY: all clean bootloader kernel drivers system iso run check check-boot

$(BUILD):
	mkdir -p $@


# Keep historical compatibility: "make" builds the two real binaries.
all: bootloader kernel

# -----------------------------------------------------------------------------
# UEFI bootloader
# -----------------------------------------------------------------------------

$(BUILD)/boot/uefi/src/boot.o: boot/uefi/src/boot.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_EFI) -MMD -MP -c $< -o $@

$(BUILD)/boot/uefi/mem.o: lib/memory/mem.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_EFI) -MMD -MP -c $< -o $@

bootloader: $(BOOT_OBJS)
	@mkdir -p $(BUILD)
	$(LD) $(LDFLAGS_EFI) -o $(BUILD)/BOOTX64.EFI $(BOOT_OBJS)
	@test -s $(BUILD)/BOOTX64.EFI
	@echo "==> UEFI bootloader: $(BUILD)/BOOTX64.EFI"

# -----------------------------------------------------------------------------
# Kernel / drivers
# -----------------------------------------------------------------------------

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_KERNEL) -MMD -MP -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_ASM) -MMD -MP -c $< -o $@

$(WALLPAPER_OBJ): assets/wallpapers/nexus_default.rgb565 | $(BUILD)
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
		--rename-section .data=.rodata,alloc,load,readonly,data,contents \
		$< $@

$(FONT_REGULAR_OBJ): assets/fonts/Roboto-Regular.ttf | $(BUILD)
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
		--rename-section .data=.rodata,alloc,load,readonly,data,contents \
		$< $@

$(FONT_BOLD_OBJ): assets/fonts/Roboto-Bold.ttf | $(BUILD)
	@mkdir -p $(dir $@)
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
		--rename-section .data=.rodata,alloc,load,readonly,data,contents \
		$< $@

# Real driver target: all driver source files have to produce their objects.
drivers: $(DRIVER_OBJS)
	@echo "==> Drivers: $(words $(DRIVER_OBJS)) object(s) ready"

kernel: $(KERNEL_OBJS)
	@mkdir -p $(BUILD)
	$(LD) $(LDFLAGS_KERNEL) -o $(BUILD)/kernel.elf $(KERNEL_OBJS)
	@test -s $(BUILD)/kernel.elf
	@echo "==> Kernel: $(BUILD)/kernel.elf"

# -----------------------------------------------------------------------------
# System integration / EFI staging
# -----------------------------------------------------------------------------

# "system" is deliberately a real target rather than a fake progress phase:
# it creates the actual files consumed by build.sh and the UEFI boot path.
system: $(SYSTEM_OUTPUTS)
	@echo "==> System staging: $(ISODIR)/"

$(ISODIR)/EFI/BOOT/BOOTX64.EFI: bootloader
	@mkdir -p $(dir $@)
	cp $(BUILD)/BOOTX64.EFI $@
	@test -s $@

$(ISODIR)/kernel.elf: kernel
	@mkdir -p $(dir $@)
	cp $(BUILD)/kernel.elf $@
	@test -s $@

# Real bootable ISO: ISO9660 + El Torito EFI boot image.
$(ISO_IMAGE): system tools/create_iso.py
	python3 tools/create_iso.py --bootloader build/BOOTX64.EFI --kernel build/kernel.elf --out $@
	@test -s $@

iso: $(ISO_IMAGE)
	@echo "==> Bootable ISO: $(ISO_IMAGE)"

# -----------------------------------------------------------------------------
# QEMU compatibility
# -----------------------------------------------------------------------------

run: iso
	./build.sh --no-prompt --run --no-img --iso-out $(ISO_IMAGE)

# -----------------------------------------------------------------------------
# Validation
# -----------------------------------------------------------------------------

check:
	@set -e; \
	for f in $$(find kernel drivers fs lib gui shell platform system -type f -name '*.c' ! -path 'kernel/bootmode/*' ! -path 'system/core/init.c' ! -path 'system/core/system.c' | sort); do \
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

# -----------------------------------------------------------------------------
# Cleanup
# -----------------------------------------------------------------------------

clean:
	rm -rf $(BUILD) $(ISODIR)

# Dependency files are generated next to their objects by -MMD/-MP.
DEPS := $(KERNEL_C_OBJS:.o=.d) $(KERNEL_S_OBJS:.o=.d) $(BOOT_OBJS:.o=.d)
-include $(DEPS)
