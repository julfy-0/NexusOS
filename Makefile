# =============================================================
#  NexusOS — Makefile (UEFI / x86_64)
# =============================================================
#  Собирается ОБЫЧНЫМ host gcc/ld — если ты на x86_64 Linux,
#  отдельный кросс-компилятор НЕ нужен (в отличие от старой
#  BIOS/i386-версии этого проекта, см. docs/adr/0002).
#
#  make            — собрать build/BOOTX64.EFI и build/kernel.elf
#  make iso        — собрать iso/ (структура ESP: EFI/BOOT/BOOTX64.EFI + kernel.elf)
#  make run        — собрать FAT-образ диска и запустить в QEMU (OVMF)
#  make clean      — удалить всё собранное
#
#  Нужны для `make run`: qemu-system-x86_64, OVMF (пакет ovmf),
#  dosfstools (mkfs.vfat), mtools (mcopy). Все — обычные пакеты
#  дистрибутива, никакой сборки toolchain не требуется.
# =============================================================

CC      := gcc
LD      := ld

BUILD   := build
ISODIR  := iso

INCLUDES := -Iinclude/nexus -Iarch/x86_64 -Ikernel -Ikernel/shell -Ikernel/shell/apps \
            -Idrivers/console -Idrivers/cpu -Idrivers/keyboard -Idrivers/pic -Idrivers/timer \
            -Idrivers/storage -Ifs -Imm

# --- Загрузчик: freestanding PE32+/EFI, MS x64 ABI ---
CFLAGS_EFI  := -ffreestanding -fno-stack-protector -fno-stack-check \
               -fshort-wchar -mno-red-zone -fpic -fno-ident \
               -Iinclude/nexus -Iboot/efi -Wall -Wextra -O2 -c
LDFLAGS_EFI := -m i386pep -nostdlib -shared -Bsymbolic -e efi_main --subsystem 10

# --- Ядро: freestanding ELF64, System V ABI, без FPU/SSE
#     (мы не настраивали CR0/CR4 для этого — как и в исходном проекте) ---
CFLAGS_KERNEL := -ffreestanding -fno-stack-protector -fno-stack-check \
                 -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -mgeneral-regs-only \
                 -fno-pic -fno-pie -fno-ident \
                 -Wall -Wextra -O2 $(INCLUDES) -c
LDFLAGS_KERNEL := -nostdlib -static -T arch/x86_64/linker.ld

.PHONY: all clean bootloader kernel iso run check

all: bootloader kernel

$(BUILD):
	mkdir -p $(BUILD)

# ---------------- Загрузчик (boot/efi) ----------------

BOOT_OBJS := $(BUILD)/boot.o $(BUILD)/mem_efi.o

$(BUILD)/boot.o: boot/efi/boot.c boot/efi/efi.h boot/efi/elf.h include/nexus/boot_info.h | $(BUILD)
	$(CC) $(CFLAGS_EFI) boot/efi/boot.c -o $@

$(BUILD)/mem_efi.o: lib/mem.c | $(BUILD)
	$(CC) $(CFLAGS_EFI) lib/mem.c -o $@

bootloader: $(BOOT_OBJS)
	$(LD) $(LDFLAGS_EFI) -o $(BUILD)/BOOTX64.EFI $(BOOT_OBJS)
	@echo "==> Загрузчик собран: $(BUILD)/BOOTX64.EFI"

# ---------------- Ядро (arch/ + kernel/ + drivers/ + fs/) ----------------

CORE_OBJS := $(BUILD)/entry.o $(BUILD)/kernel.o \
             $(BUILD)/gdt.o $(BUILD)/gdt_asm.o $(BUILD)/idt.o $(BUILD)/isr.o \
             $(BUILD)/paging.o \
             $(BUILD)/kstate.o $(BUILD)/mem_kernel.o

FS_OBJS := $(BUILD)/pci.o $(BUILD)/ahci.o $(BUILD)/fat32.o

DRIVER_OBJS := $(BUILD)/console.o $(BUILD)/pic.o $(BUILD)/cpu.o $(BUILD)/keyboard.o $(BUILD)/pit.o

APP_NAMES := vfs neofetch sysinfo meminfo about whoami version date echo reverse len \
             upper lower title calc sum hex dec isprime fib \
             ls pwd cd mkdir rmdir touch rm cp mv \
             cat less head tail grep diff find \
             write append wc df du colors beep \
             reboot halt shutdown uname man \
             lspci uptime diskls diskcat

SHELL_OBJS := $(BUILD)/shell.o $(patsubst %,$(BUILD)/%.o,$(APP_NAMES))

KERNEL_OBJS := $(CORE_OBJS) $(FS_OBJS) $(DRIVER_OBJS) $(SHELL_OBJS)

# -- arch/x86_64 (asm) --
$(BUILD)/entry.o: arch/x86_64/entry.S | $(BUILD)
	$(CC) -ffreestanding -c arch/x86_64/entry.S -o $@

$(BUILD)/gdt_asm.o: arch/x86_64/gdt_asm.S | $(BUILD)
	$(CC) -ffreestanding -c arch/x86_64/gdt_asm.S -o $@

$(BUILD)/isr.o: arch/x86_64/isr.S | $(BUILD)
	$(CC) -ffreestanding -c arch/x86_64/isr.S -o $@

# -- arch/x86_64 (C) --
$(BUILD)/gdt.o: arch/x86_64/gdt.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) arch/x86_64/gdt.c -o $@

$(BUILD)/idt.o: arch/x86_64/idt.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) arch/x86_64/idt.c -o $@

# -- mm/ --
$(BUILD)/paging.o: mm/paging.c mm/paging.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) mm/paging.c -o $@

# -- kernel/ --
$(BUILD)/kernel.o: kernel/kernel.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) kernel/kernel.c -o $@

$(BUILD)/kstate.o: kernel/kstate.c kernel/kstate.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) kernel/kstate.c -o $@

$(BUILD)/mem_kernel.o: lib/mem.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) lib/mem.c -o $@

$(BUILD)/shell.o: kernel/shell/shell.c kernel/shell/shell.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) kernel/shell/shell.c -o $@

define APP_RULE
$(BUILD)/$(1).o: kernel/shell/apps/$(1).c kernel/shell/apps/$(1).h | $(BUILD)
	$$(CC) $$(CFLAGS_KERNEL) kernel/shell/apps/$(1).c -o $$@
endef
$(foreach app,$(APP_NAMES),$(eval $(call APP_RULE,$(app))))

# -- fs/ + drivers/storage (диск и файловые системы) --
$(BUILD)/pci.o: drivers/storage/pci.c drivers/storage/pci.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) drivers/storage/pci.c -o $@

$(BUILD)/ahci.o: drivers/storage/ahci.c drivers/storage/ahci.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) drivers/storage/ahci.c -o $@

$(BUILD)/fat32.o: fs/fat32.c fs/fat32.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) fs/fat32.c -o $@

# -- drivers/ (остальные) --
$(BUILD)/console.o: drivers/console/console.c drivers/console/font8x16.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) drivers/console/console.c -o $@

$(BUILD)/pic.o: drivers/pic/pic.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) drivers/pic/pic.c -o $@

$(BUILD)/pit.o: drivers/timer/pit.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) drivers/timer/pit.c -o $@

$(BUILD)/cpu.o: drivers/cpu/cpu.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) drivers/cpu/cpu.c -o $@

$(BUILD)/keyboard.o: drivers/keyboard/keyboard.c drivers/keyboard/keyboard.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) drivers/keyboard/keyboard.c -o $@

kernel: $(KERNEL_OBJS)
	$(LD) $(LDFLAGS_KERNEL) -o $(BUILD)/kernel.elf $(KERNEL_OBJS)
	@echo "==> Ядро собрано: $(BUILD)/kernel.elf"

# ---------------- Образ ESP (EFI System Partition) ----------------

iso: bootloader kernel
	mkdir -p $(ISODIR)/EFI/BOOT
	cp $(BUILD)/BOOTX64.EFI $(ISODIR)/EFI/BOOT/BOOTX64.EFI
	cp $(BUILD)/kernel.elf $(ISODIR)/kernel.elf
	@echo "==> $(ISODIR)/ готов (структура ESP)"

# Требует qemu-system-x86_64, OVMF, dosfstools (mkfs.vfat), mtools (mcopy).
run: iso
	mkdir -p $(BUILD)
	dd if=/dev/zero of=$(BUILD)/fat.img bs=1M count=64 status=none
	mkfs.vfat -F 32 $(BUILD)/fat.img >/dev/null
	mcopy -i $(BUILD)/fat.img -s $(ISODIR)/* ::/
	@if [ ! -f OVMF_VARS.fd ]; then \
		cp /usr/share/OVMF/OVMF_VARS_4M.fd ./OVMF_VARS.fd; \
	fi
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
		-drive if=pflash,format=raw,file=OVMF_VARS.fd \
		-drive format=raw,file=$(BUILD)/fat.img \
		-m 256M

# Быстрая проверка синтаксиса всех .c без реальной сборки.
check:
	@for f in $$(find boot kernel drivers fs lib mm -name '*.c'); do \
		$(CC) $(CFLAGS_KERNEL) -fsyntax-only $$f || exit 1; \
	done
	@echo "==> Синтаксис в порядке"

clean:
	rm -rf $(BUILD) $(ISODIR)
