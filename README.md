# NexusOS

Самостоятельная 64-битная ОС для x86_64, написанная с нуля на C, со
своим UEFI-загрузчиком (без GRUB). Монолитное ядро (long mode), свой
GDT/IDT/PIC/PIT, PS/2-клавиатура, консоль на framebuffer, диск через
AHCI+FAT32, встроенный шелл с ~50 командами.

Собирается **обычным host `gcc`/`ld`** — если ты на x86_64 Linux,
отдельный кросс-компилятор не нужен.

> Текущая версия — **0.4.6-memoria** (см. `docs/STATUS.md`). Версия
> 0.3.0-refit была архитектурным пивотом с прежней BIOS/i386/GRUB
> версии на UEFI/x86_64. История и причина — `docs/adr/0002-uefi-x86_64-pivot.md`.

## Структура проекта

```
NexusOS/
├── Makefile
├── OVMF_VARS.fd              # NVRAM-переменные UEFI-прошивки для QEMU
├── boot/efi/                 # UEFI-загрузчик (PE32+, свой ELF64-парсер, без GRUB)
├── arch/x86_64/               # entry point, GDT, IDT, ISR, linker script, io.h
├── kernel/
│   ├── kernel.c / kstate.h    # kmain, глобальный доступ к boot_info
│   └── shell/                 # встроенный шелл + apps/ (~50 команд: ls, cat,
│                               #   grep, calc, neofetch, reboot, diskls, ...)
├── drivers/
│   ├── console/                # framebuffer + битмап-шрифт 8x16
│   ├── cpu/                    # CPUID (vendor/brand/логические ядра)
│   ├── keyboard/                # PS/2, полная инициализация i8042
│   ├── pic/                     # 8259 remap + EOI
│   ├── timer/                   # PIT (IRQ0)
│   └── storage/                 # PCI enumeration, AHCI (SATA)
├── fs/                        # FAT32 (монтирование, чтение)
├── lib/mem.c                  # freestanding memcpy/memset/strlen/...
├── include/nexus/boot_info.h  # контракт bootloader ↔ kernel
├── userdata/                  # заготовки под пользовательские данные
└── docs/
    ├── STATUS.md              # ЧИТАТЬ ПЕРВЫМ — текущее состояние
    ├── AI_HANDOFF.md          # правила для продолжающего (человек или AI)
    ├── ROADMAP.md             # путь развития по milestone'ам
    ├── VERSIONING.md          # наша схема версий
    ├── MIGRATION_0002.md      # таблица старые-пути → новые-пути (пивот)
    ├── adr/                   # архитектурные решения и почему
    ├── ARCHITECTURE.md        # поток загрузки, прерывания, память
    └── BUILDING.md            # как собрать и запустить
```

## Быстрый старт

```bash
make            # build/BOOTX64.EFI + build/kernel.elf
make run        # + образ диска, запуск в QEMU (нужны qemu-system-x86_64,
                #   ovmf, dosfstools, mtools — apt install, без сборки toolchain)
```

Подробности, известные проблемы и как их лечить — `docs/BUILDING.md`.

## Продолжаешь с другой нейронкой или через месяц?

Начни с `docs/STATUS.md`. Эта система (STATUS/ROADMAP/ADR/AI_HANDOFF)
придумана специально, чтобы не зависеть от памяти конкретной AI-сессии
— читай `docs/AI_HANDOFF.md` перед тем, как вносить изменения.
