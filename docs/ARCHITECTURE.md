# ARCHITECTURE.md

## Тип ядра

Монолитное (`docs/adr/0001`). Архитектура — UEFI/x86_64
(`docs/adr/0002`, заменяет прежнюю BIOS/i386/GRUB).

## Организация дерева исходников

```text
boot/uefi/        UEFI loader
kernel/core/      kernel orchestration, state, panic, usermode foundation
kernel/arch/      x86_64 entry/GDT/IDT/ISR/linker
kernel/mm/        paging
 drivers/         hardware/input/storage/graphics/timer/USB
fs/vfs/           VFS core, mounts, filesystem registry
fs/fat32/         FAT32 implementation
gui/              GUI core + renderer/font boundary
shell/            shell core + categorized commands
lib/memory/       freestanding memory primitives
assets/           wallpapers and fonts
include/nexus/    shared headers
platform/target/  hardware detection
```

The directory boundaries are organizational boundaries only. The current
kernel remains monolithic and all linked kernel-side C modules execute in
kernel context.

## Поток загрузки

```text
UEFI Firmware
  → boot/uefi/boot.c: efi_main(ImageHandle, SystemTable)
      1. GOP → framebuffer
      2. открывается `\kernel.elf` на загрузочном томе
      3. ELF64 валидируется
      4. все PT_LOAD сегменты получают единый физический image allocation
         ниже 4 GiB
      5. ET_DYN `R_X86_64_RELATIVE` relocations применяются на месте
      6. boot_info + финальная EFI memory map фиксируются ниже 4 GiB
      7. ExitBootServices()
      8. переход на runtime-relocated kernel entry с `RDI = boot_info`
  → kernel/arch/x86_64/entry.S: `_start`
      собственный 64 KiB stack → `kmain(boot_info)`
  → kernel/core/kernel.c
      contract validation → GDT → IDT → own paging → PMM → VMM → heap →
      PIC/PIT/input/storage/USB/GPU → shell/usermode/GUI → STI
```

### Boot contract

`nexus_boot_info_t` теперь содержит не только framebuffer и EFI memory map,
но и фактический runtime physical range ядра: `kernel_phys_base`,
`kernel_phys_end`, `kernel_image_size`, `kernel_link_base` и `kernel_entry`.
Это принципиально важно: после relocatable загрузки linker address больше
не равен physical address.

### Kernel image model

NexusOS остаётся freestanding x86_64 kernel, но ELF теперь собирается как
`ET_DYN` с `-fPIE`. Текущая loader contract допускает только relocations
`R_X86_64_RELATIVE`; `readelf -r` для текущего kernel image показывает
только этот тип relocation. Никакой userspace dynamic linker после hand-off
не нужен.


## Прерывания

IDT на 48 векторов: 0-31 исключения CPU, 32-47 — IRQ0-15 после PIC
remap (`drivers/hardware/pic/pic.c`). Общий ассемблерный стаб
(`kernel/arch/x86_64/isr.S`) сохраняет регистры в `interrupt_frame_t`
(layout зафиксирован в `kernel/arch/x86_64/idt.h` — при изменении стаба
менять оба места синхронно) и зовёт единый C-обработчик
`isr_handler()` (`kernel/arch/x86_64/idt.c`), который либо паникует
(vector < 32), либо диспетчеризует по номеру (32 = таймер,
33 = клавиатура), либо просто шлёт EOI.

Это отличается от типичного паттерна "таблица callback'ов,
регистрируемая динамически" (как было в прежней BIOS-версии) — здесь
диспетчеризация зашита прямо в `isr_handler()` через `if`. Для
текущего набора устройств (2 IRQ) это осознанно проще; если устройств
станет больше — рассмотреть переход на таблицу, но это не сделано
заранее ("не оптимизируй то, что не болит").

## Память

Свои page tables (`kernel/mm/paging.c`) сохраняют identity mapping первых
4 GiB с 2 MiB страницами. Это сознательно оставлено простым после rewrite:
relocatable kernel может находиться в любом физическом месте внутри этого
диапазона, а PMM резервирует именно runtime image range из boot contract.
VMM продолжает добавлять 4 KiB mappings для heap и других виртуальных регионов.

Текущая архитектура ещё не является higher-half kernel. Адрес `0x200000` —
link-time base ELF image, а не обязательный physical load address. Настоящий
higher-half переход остаётся отдельной задачей roadmap.


## Драйверы и связи между модулями

`drivers/input/keyboard/keyboard.c` напрямую зовёт `shell_input_char()` —
то есть драйвер клавиатуры знает о существовании шелла. Это отличается
от принципа "драйвер ничего не знает о том, кто его использует" из
`docs/adr/0001` — но так было в исходном перенесённом проекте, и
переписывать это заодно с переносом было бы смешиванием двух разных
задач в одном шаге. Разрыв этой связи (через очередь событий) — явный
пункт в `docs/ROADMAP.md` (Milestone "threadwork"), не забыт, просто
отложен.

## Шелл и команды

`shell/shell.c` + `shell/apps/*.c` — почти 50 команд,
каждая как отдельная пара `.c`/`.h`. Все они выполняются **в контексте
прерывания клавиатуры**, синхронно, в кольце 0 — не как процессы.
Команда должна быть быстрой и не блокирующей (см. комментарий в шапке
`shell.c`). Это временно: Milestone "descent" должен вынести шелл в
user-space процесс без переписывания логики команд с нуля.
