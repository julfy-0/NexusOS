# CHANGELOG.md

Формат: `## [версия] — codename` + список изменений. Новое — сверху.

## [0.1.2] — genesis

- Причина повторной коллизии `isr.o`/`irq.o` после обновления 0.1.1:
  распаковка нового архива поверх старой папки не удаляет файлы,
  которых нет в новом архиве — старые `arch/i386/isr.S`/`irq.S`
  оставались рядом с новыми `isr_stubs.S`/`irq_stubs.S`
- Makefile: добавлена явная проверка на коллизии путей `.o` перед
  сборкой (`DUPLICATE_OBJECTS`) — теперь такая ситуация даёт понятную
  ошибку сразу, а не загадочный "multiple definition" от линкера

## [0.1.1] — genesis

- Фикс: `arch/i386/isr.S` и `arch/i386/irq.S` переименованы в
  `isr_stubs.S`/`irq_stubs.S` — они собирались в те же объектники
  (`isr.o`/`irq.o`), что и одноимённые `isr.c`/`irq.c`, из-за чего
  линковщик ловил "multiple definition" и одновременно не находил
  `isr0..isr31`/`irq0..irq15` (реальный кросс-компилятор `i686-elf-gcc`
  вскрыл это на первой попытке `make iso` — sanity-check host-gcc'ом
  эту коллизию не поймал, потому что там объектники называлась иначе)

## [0.1.0] — genesis

- Multiboot boot-стаб (boot/boot.S), передача управления в kernel_main
- GDT: плоская модель, kernel/user code/data сегменты
- IDT + ISR: обработчики всех 32 исключений CPU с диагностикой
- IRQ: remap PIC на векторы 32-47, диспетчер обработчиков
- Драйверы: VGA text-mode, serial COM1, PS/2 keyboard, PIT timer
- kprintf (%d %u %x %s %c %%), вывод одновременно на VGA и serial
- PMM: bitmap physical memory manager по Multiboot memory map
- panic() — аварийная остановка с диагностикой
- Инфраструктура проекта: Makefile, ADR, ROADMAP, VERSIONING, STATUS
