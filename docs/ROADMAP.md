# ROADMAP.md — путь развития NexusOS

Каждый Milestone — это `+1` к MINOR-версии (см. `docs/VERSIONING.md`).
Внутри milestone — задачи в порядке, в котором их реально можно делать
(каждая опирается на предыдущую). Не перескакивай порядок — почти всё
дальше зависит от paging и multitasking.

## Milestone 0.1 — genesis (Boot & Core) ✅ структура готова

- [x] Multiboot boot, GDT, IDT, ISR, IRQ, PIC remap
- [x] VGA + serial вывод, kprintf
- [x] PS/2 keyboard, PIT timer
- [x] PMM (bitmap physical memory)
- [ ] **Подтвердить живой бут в QEMU** — единственное, что осталось
      для закрытия milestone формально

## Milestone 0.2 — memoria (Virtual Memory)

- [ ] Paging: page directory + page tables, 4 KiB страницы
- [ ] Higher-half kernel (переезд на 0xC0000000) — стандартный
      hobby-OS паттерн, упрощает будущий user/kernel split
- [ ] Page fault handler (ISR 14) с осмысленной диагностикой
- [ ] `kmalloc`/`kfree` — heap ядра на основе paging + PMM
- [ ] Copy-on-write — можно отложить до multitasking, но
      закладывать API с расчётом на это

## Milestone 0.3 — threadwork (Многозадачность)

- [ ] Структура `task_t` / TCB (task control block)
- [ ] Переключение контекста (context switch) — ассемблер неизбежен
- [ ] Простой round-robin планировщик на таймере (IRQ0 уже есть)
- [ ] `sleep()`/блокировка задач, примитивы синхронизации (спинлоки)
- [ ] Kernel threads (ещё не user-mode процессы — это Milestone 0.5)

## Milestone 0.4 — archive (Хранение данных)

- [ ] Драйвер ATA (PIO режим — простейший, AHCI можно позже)
- [ ] VFS-слой (виртуальная файловая система, абстракция над ФС)
- [ ] Простая ФС — либо своя минимальная, либо FAT32 (совместимость
      с реальными флешками/образами для тестов)
- [ ] Инициализация initrd/ramdisk для раннего теста без диска

## Milestone 0.5 — descent (User Mode)

- [ ] Переход в ring 3, TSS (Task State Segment)
- [ ] Системные вызовы (int 0x80 или syscall/sysret)
- [ ] Загрузка ELF-бинарников из ФС в user-space процесс
- [ ] Минимальный libc для user-space программ (свой, freestanding)
- [ ] Простой shell как первая user-space программа

## Дальше (не расписано подробно — решим ближе к делу)

- Сеть (виртуальный NIC в QEMU для начала — e1000/rtl8139)
- SMP (несколько ядер CPU)
- Полноценная ФС с журналированием
- Пакетный менеджер / порты для сборки сторонних программ

## Как решаем, что "готово" на 1.0

Не по чекбоксам автоматически — отдельным ADR, когда система реально
самодостаточна: грузится, крутит несколько user-space процессов,
имеет ФС и интерактивный shell, на котором можно что-то реально
сделать (не просто echo).
