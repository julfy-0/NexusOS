# =============================================================
#  NexusOS — главный Makefile
# =============================================================
#  make            — собрать ядро (build/nexuskernel.bin)
#  make iso         — собрать загрузочный build/NexusOS.iso
#  make run         — запустить ISO в QEMU
#  make run-kernel  — запустить сырое ядро в QEMU (без ISO, быстрее)
#  make clean       — удалить всё собранное
#  make check       — sanity-компиляция каждого .c хостовым gcc
#                      (не линковка, не бут — только проверка синтаксиса,
#                      удобно когда под рукой нет кросс-компилятора)
# =============================================================

PROJECT   := NexusOS
BUILD_DIR := build
ISO_DIR   := $(BUILD_DIR)/isodir

# Кросс-компилятор. Если у тебя ещё не собран i686-elf-gcc,
# смотри toolchain/build-cross-compiler.sh и docs/BUILDING.md.
CC  := i686-elf-gcc
AS  := i686-elf-gcc
LD  := i686-elf-gcc

CFLAGS := -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector \
          -nostdlib -Wall -Wextra -O2 -g -I. -Iinclude
ASFLAGS := -ffreestanding -I.
LDFLAGS := -ffreestanding -O2 -nostdlib -lgcc -T arch/i386/linker.ld

# --- Исходники -------------------------------------------------
C_SOURCES := $(shell find boot arch kernel drivers mm lib -name '*.c')
S_SOURCES := $(shell find boot arch kernel drivers mm lib -name '*.S')

C_OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
S_OBJECTS := $(patsubst %.S,$(BUILD_DIR)/%.o,$(S_SOURCES))
OBJECTS   := $(C_OBJECTS) $(S_OBJECTS)

# Защита от "тихих" коллизий: если .c и .S файл с одинаковым именем
# (например old_isr.c и leftover_isr.S) целятся в один и тот же путь
# .o, линкер потом падает с малопонятным "multiple definition".
# Ловим это здесь явно, с понятным сообщением, до самой сборки.
DUPLICATE_OBJECTS := $(shell echo $(OBJECTS) | tr ' ' '\n' | sort | uniq -d)
ifneq ($(strip $(DUPLICATE_OBJECTS)),)
$(error Коллизия имён объектников — один и тот же путь .o собирается из двух разных исходников: $(DUPLICATE_OBJECTS). Обычно это значит, что рядом лежат и .c, и .S файл с одинаковым базовым именем (например после ручной распаковки архива поверх старой версии остались лишние файлы). Проверь: find . -name '*.c' -o -name '*.S' | sort, найди пары с одинаковым basename и удали лишний файл)
endif

KERNEL := $(BUILD_DIR)/nexuskernel.bin
ISO    := $(BUILD_DIR)/$(PROJECT).iso

.PHONY: all iso run run-kernel clean check toolchain-info

all: $(KERNEL)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

$(KERNEL): $(OBJECTS) arch/i386/linker.ld
	@mkdir -p $(dir $@)
	$(LD) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "==> Ядро собрано: $@"

iso: $(KERNEL)
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/nexuskernel.bin
	cp config/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) $(ISO_DIR)
	@echo "==> ISO собран: $(ISO)"

run: iso
	qemu-system-i386 -cdrom $(ISO) -serial stdio

run-kernel: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -serial stdio

clean:
	rm -rf $(BUILD_DIR)

# Быстрая проверка синтаксиса без кросс-компилятора. Не собирает
# рабочий образ (нет линковки под i386-elf), но ловит опечатки и
# ошибки типов ещё до того, как ты соберёшь toolchain.
check:
	@for f in $(C_SOURCES); do \
		gcc -m32 -std=gnu11 -ffreestanding -fno-builtin -fsyntax-only \
		    -Wall -Wextra -I. -Iinclude $$f || exit 1; \
	done
	@echo "==> Синтаксис всех .c файлов в порядке (host gcc -fsyntax-only)"

toolchain-info:
	@echo "Нужен i686-elf-gcc в PATH. Собрать: ./toolchain/build-cross-compiler.sh"
