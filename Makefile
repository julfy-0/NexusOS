CC      = gcc
LD      = ld
OBJCOPY = objcopy

BUILD   = build
ISO     = iso

# ============================================================
# 5 разделов проекта (по аналогии с разделами Android):
#   boot/         — то, что поднимает машину:
#     boot/efi/     — EFI-бутлоадер (отдельный мир, PE32+, MS x64 ABI)
#     boot/kernel/  — ядро, которое бутлоадер грузит и запускает (ELF, SysV ABI)
#   vendor/       — драйверы железа (кроме диска — см. ms/): input/, display/,
#                   interrupt/, timer/, cpu/
#   system/       — встроенные системные программы и сервисы (shell, apps/*)
#   userdata/     — заготовки под пользовательские данные и программы
#   ms/           — mount storage: всё про диск и файловые системы
#     ms/storage/   — драйверы диска (PCI-перечисление, AHCI)
#     ms/fs/        — файловые системы поверх ms/storage (FAT32)
#   common/       — общее между boot/efi и boot/kernel (boot_info.h, mem.c)
# ============================================================

INCLUDES = -Icommon -Iboot/kernel \
           -Ivendor -Ivendor/input -Ivendor/display -Ivendor/interrupt \
           -Ivendor/timer -Ivendor/cpu \
           -Ims/storage -Ims/fs \
           -Isystem -Isystem/apps

# --- Бутлоадер: freestanding PE32+/EFI, MS x64 ABI ---
CFLAGS_EFI  = -ffreestanding -fno-stack-protector -fno-stack-check \
              -fshort-wchar -mno-red-zone -fpic -fno-ident \
              -Icommon -Wall -Wextra -O2 -c
LDFLAGS_EFI = -m i386pep -nostdlib -shared -Bsymbolic \
              -e efi_main --subsystem 10

# --- Ядро (и все его слои): обычный freestanding ELF, System V ABI,
#     без FPU/SSE (мы не настраивали CR0/CR4 для этого) ---
CFLAGS_KERNEL = -ffreestanding -fno-stack-protector -fno-stack-check \
                -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -mgeneral-regs-only \
                -fno-pic -fno-pie -fno-ident \
                -Wall -Wextra -O2 $(INCLUDES) -c
LDFLAGS_KERNEL = -nostdlib -static -T boot/kernel/kernel.ld

.PHONY: all clean bootloader kernel iso run

all: iso

$(BUILD):
	mkdir -p $(BUILD)

# ---------------- boot/efi (бутлоадер) ----------------

BOOT_OBJS = $(BUILD)/boot.o $(BUILD)/mem_efi.o

$(BUILD)/boot.o: boot/efi/boot.c boot/efi/efi.h boot/efi/elf.h common/boot_info.h | $(BUILD)
	$(CC) $(CFLAGS_EFI) boot/efi/boot.c -o $@

$(BUILD)/mem_efi.o: common/mem.c | $(BUILD)
	$(CC) $(CFLAGS_EFI) common/mem.c -o $@

bootloader: $(BOOT_OBJS)
	$(LD) $(LDFLAGS_EFI) -o $(BUILD)/BOOTX64.EFI $(BOOT_OBJS)

# ---------------- Kernel (boot/kernel/ + vendor/ + ms/ + system/) ----------------

KERNEL_CORE_OBJS = $(BUILD)/entry.o $(BUILD)/kernel.o \
                    $(BUILD)/gdt.o $(BUILD)/gdt_asm.o $(BUILD)/idt.o $(BUILD)/isr.o \
                    $(BUILD)/kstate.o $(BUILD)/mem_kernel.o

MS_OBJS = $(BUILD)/pci.o $(BUILD)/ahci.o $(BUILD)/fat32.o

VENDOR_OBJS = $(BUILD)/console.o $(BUILD)/pic.o $(BUILD)/cpu.o $(BUILD)/keyboard.o \
              $(BUILD)/pit.o

SYSTEM_OBJS = $(BUILD)/shell.o $(BUILD)/vfs.o \
              $(BUILD)/neofetch.o $(BUILD)/sysinfo.o $(BUILD)/about.o \
              $(BUILD)/whoami.o $(BUILD)/version.o $(BUILD)/date.o \
              $(BUILD)/echo.o $(BUILD)/reverse.o $(BUILD)/len.o \
              $(BUILD)/upper.o $(BUILD)/lower.o $(BUILD)/title.o \
              $(BUILD)/calc.o $(BUILD)/sum.o $(BUILD)/hex.o $(BUILD)/dec.o \
              $(BUILD)/isprime.o $(BUILD)/fib.o \
              $(BUILD)/ls.o $(BUILD)/pwd.o $(BUILD)/cd.o \
              $(BUILD)/mkdir.o $(BUILD)/rmdir.o $(BUILD)/touch.o $(BUILD)/rm.o \
              $(BUILD)/cp.o $(BUILD)/mv.o \
              $(BUILD)/cat.o $(BUILD)/less.o $(BUILD)/head.o $(BUILD)/tail.o \
              $(BUILD)/grep.o $(BUILD)/diff.o $(BUILD)/find.o \
              $(BUILD)/write.o $(BUILD)/append.o $(BUILD)/wc.o \
              $(BUILD)/df.o $(BUILD)/du.o \
              $(BUILD)/colors.o $(BUILD)/beep.o \
              $(BUILD)/reboot.o $(BUILD)/halt.o $(BUILD)/shutdown.o \
              $(BUILD)/uname.o $(BUILD)/man.o \
              $(BUILD)/lspci.o $(BUILD)/uptime.o $(BUILD)/diskls.o $(BUILD)/diskcat.o

KERNEL_OBJS = $(KERNEL_CORE_OBJS) $(MS_OBJS) $(VENDOR_OBJS) $(SYSTEM_OBJS)

# -- boot/kernel/ --
$(BUILD)/entry.o: boot/kernel/entry.S | $(BUILD)
	$(CC) -ffreestanding -c boot/kernel/entry.S -o $@

$(BUILD)/gdt_asm.o: boot/kernel/gdt_asm.S | $(BUILD)
	$(CC) -ffreestanding -c boot/kernel/gdt_asm.S -o $@

$(BUILD)/isr.o: boot/kernel/isr.S | $(BUILD)
	$(CC) -ffreestanding -c boot/kernel/isr.S -o $@

$(BUILD)/kernel.o: boot/kernel/kernel.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) boot/kernel/kernel.c -o $@

$(BUILD)/gdt.o: boot/kernel/gdt.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) boot/kernel/gdt.c -o $@

$(BUILD)/idt.o: boot/kernel/idt.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) boot/kernel/idt.c -o $@

$(BUILD)/kstate.o: boot/kernel/kstate.c boot/kernel/kstate.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) boot/kernel/kstate.c -o $@

$(BUILD)/mem_kernel.o: common/mem.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) common/mem.c -o $@

# -- ms/ (mount storage: диск + файловые системы) --
$(BUILD)/pci.o: ms/storage/pci.c ms/storage/pci.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) ms/storage/pci.c -o $@

$(BUILD)/ahci.o: ms/storage/ahci.c ms/storage/ahci.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) ms/storage/ahci.c -o $@

$(BUILD)/fat32.o: ms/fs/fat32.c ms/fs/fat32.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) ms/fs/fat32.c -o $@

# -- vendor/ (драйверы, по подпапкам) --
$(BUILD)/console.o: vendor/display/console.c vendor/display/font8x16.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) vendor/display/console.c -o $@

$(BUILD)/pic.o: vendor/interrupt/pic.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) vendor/interrupt/pic.c -o $@

$(BUILD)/pit.o: vendor/timer/pit.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) vendor/timer/pit.c -o $@

$(BUILD)/cpu.o: vendor/cpu/cpu.c | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) vendor/cpu/cpu.c -o $@

$(BUILD)/keyboard.o: vendor/input/keyboard.c vendor/input/keyboard.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) vendor/input/keyboard.c -o $@

# -- system/ (встроенные программы) --
$(BUILD)/shell.o: system/shell.c system/shell.h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) system/shell.c -o $@

define APP_RULE
$(BUILD)/$(1).o: system/apps/$(1).c system/apps/$(1).h | $(BUILD)
	$(CC) $(CFLAGS_KERNEL) system/apps/$(1).c -o $$@
endef

$(foreach app,vfs neofetch sysinfo about whoami version date echo reverse len \
              upper lower title calc sum hex dec isprime fib \
              ls pwd cd mkdir rmdir touch rm cp mv \
              cat less head tail grep diff find \
              write append wc df du colors beep \
              reboot halt shutdown uname man \
              lspci uptime diskls diskcat, \
  $(eval $(call APP_RULE,$(app))))

kernel: $(KERNEL_OBJS)
	$(LD) $(LDFLAGS_KERNEL) -o $(BUILD)/kernel.elf $(KERNEL_OBJS)

# ---------------- Образ диска (структура ESP) ----------------

iso: bootloader kernel
	mkdir -p $(ISO)/EFI/BOOT
	cp $(BUILD)/BOOTX64.EFI $(ISO)/EFI/BOOT/BOOTX64.EFI
	cp $(BUILD)/kernel.elf $(ISO)/kernel.elf
	@echo "Каталог $(ISO) готов — это структура ESP (EFI System Partition)."

# Требует qemu-system-x86_64, OVMF, dosfstools (mkfs.vfat) и mtools (mcopy).
# ЯВНО зависит от iso — раньше `make run` без предварительного `make iso`
# падал в рантайме, потому что iso/ ещё не существовал.
run: iso
	mkdir -p $(BUILD)
	dd if=/dev/zero of=$(BUILD)/fat.img bs=1M count=64 status=none
	mkfs.vfat -F 32 $(BUILD)/fat.img >/dev/null
	mcopy -i $(BUILD)/fat.img -s $(ISO)/* ::/
	@if [ ! -f OVMF_VARS.fd ]; then \
		cp /usr/share/OVMF/OVMF_VARS_4M.fd ./OVMF_VARS.fd; \
	fi
	qemu-system-x86_64 \
		-drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
		-drive if=pflash,format=raw,file=OVMF_VARS.fd \
		-drive format=raw,file=$(BUILD)/fat.img \
		-m 256M

clean:
	rm -rf $(BUILD) $(ISO)
