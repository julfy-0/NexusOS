NEXUSOS — MASTER CONTEXT / HANDOFF PROMPT

Ты продолжаешь разработку существующей операционной системы NexusOS.

GitHub:
https://github.com/julfy-0/NexusOS

Автор/разработчик: julfy-0

==================================================
1. ГЛАВНОЕ ПРАВИЛО
==================================================

NexusOS — это УЖЕ СУЩЕСТВУЮЩИЙ проект.

НЕ создавай новую ОС с нуля.
НЕ переписывай проект полностью без необходимости.
НЕ заменяй существующую архитектуру на свою только потому, что она кажется лучше.
НЕ создавай новый проект.

Перед любыми изменениями:
1. Изучи текущий исходный код.
2. Изучи README.md.
3. Изучи CHANGELOG.md.
4. Изучи docs/ROADMAP.md.
5. Изучи docs/STATUS.md.
6. Изучи текущую архитектуру kernel/scheduler/bootloader.
7. Изучи изменяемый subsystem.
8. Найди существующие API.
9. Определи зависимости между изменяемыми компонентами.
10. Только после этого вноси изменения.

Если предоставлен ZIP проекта — сначала распакуй и проанализируй ВЕСЬ проект.
Реальный исходный код всегда имеет приоритет над этим prompt.

==================================================
2. ТЕКУЩАЯ ВЕРСИЯ
==================================================

Текущая версия:
NexusOS 0.5.9 — Enstein

Техническая версия:
0.5.9

Display version:
0.5.9 - Enstein

Последний текущий milestone:
0.5.9 — Networking & Security Foundation

Следующий этап НЕ считать начатым автоматически.

==================================================
3. ИСТОРИЯ 0.5.3 — KERNEL SCHEDULING & SYNCHRONIZATION
==================================================

Завершено:
[x] 0.5.3.1 — Interrupt/Event Queue
[x] 0.5.3.2 — Timer-driven Scheduler Foundation
[x] 0.5.3.3 — Threads & TCB
[x] 0.5.3.4 — Ready Queue & Sleep/Wakeup
[x] 0.5.3.5 — Synchronization Primitives
[x] 0.5.3.6 — Process-safe Event Integration

0.5.3.1 — Interrupt/Event Queue
- fixed event queue
- IRQ handlers работают как capture-only
- обработка событий переносится в normal context
- PIT/keyboard/mouse интегрированы через event queue
- xHCI polling не должен выполнять тяжёлую обработку в IRQ

0.5.3.2 — Timer-driven Scheduler Foundation
- PIT timer
- scheduler ticks
- quantum accounting
- quantum expiration
- context switch statistics
- reschedule requests

Context switching из IRQ НЕ выполняется.

0.5.3.3 — Threads & TCB
Добавлены реальные kernel threads.
Есть TCB, thread ID, kernel stacks, context switching, thread_create(), thread_yield(), thread_exit().
TID 0 — bootstrap/main kernel thread.

Context switch:
- сохраняет callee-saved registers
- сохраняет RSP
- переключает stack/context
- выполняется только из normal context

0.5.3.4 — Ready Queue & Sleep/Wakeup
- Ready Queue
- Sleep Queue
- THREAD_SLEEPING
- thread_sleep_ms()
- wakeup based on scheduler ticks
- zombie cleanup/reaping

0.5.3.5 — Synchronization Primitives
Spinlock:
- spinlock_lock()
- spinlock_try_lock()
- spinlock_unlock()
- spinlock_lock_irqsave()
- spinlock_unlock_irqrestore()

Mutex:
- mutex_lock()
- mutex_try_lock()
- mutex_unlock()

Также:
- THREAD_BLOCKED
- wait queues
- thread blocking/wakeup

0.5.3.6 — Process-safe Event Integration
- kernel_events_wait()
- sequence counters
- wait queues
- timer waiting
- keyboard waiting
- mouse waiting

Использовать Sequence Counter + Wait Queue для предотвращения lost-event race.

==================================================
4. NEXUSOS 0.5.4 — PROCESSES & USERSPACE FOUNDATION
==================================================

Добавлено:
- Process subsystem
- PID
- Process states
- Process lifecycle
- Current process tracking
- User execution metadata
- User virtual memory foundation
- User page allocation/mapping foundation
- TSS RSP0
- Ring 3 transition foundation
- IRETQ transition foundation
- syscall dispatch foundation

Syscall foundation:
- NOP
- GETPID
- EXIT

НЕ заявлять полноценный userspace, если его нет в реальном исходнике.
Не считать автоматически готовыми:
- private PML4/CR3 per process
- полноценную address-space isolation
- полноценный ELF loader
- полноценный user init
- полноценные runnable user programs
- полноценный POSIX API

==================================================
5. NEXUSOS 0.5.5 — SHELL & I/O FOUNDATION
==================================================

Shell parser поддерживает:
- arguments
- single/double quotes
- escape sequences
- ;
- &&
- |
- <
- >
- >>

Добавлена I/O execution foundation:
- output capture
- pipeline foundation
- input/output redirection foundation
- append redirection
- VFS integration

НЕ заявлять POSIX file descriptors или полноценный writable filesystem, если их реально нет.

==================================================
6. NEXUSOS 0.5.6 — UNIFIED INPUT & USB HID
==================================================

Unified Input sources:
- PS/2 Keyboard
- PS/2 Mouse
- USB HID Keyboard
- USB HID Mouse

Архитектура:
Hardware
↓
IRQ/Event Capture
↓
Normal Kernel Context
↓
Input Core
↓
Shell / GUI

НЕ переносить сложную input обработку обратно в IRQ.
НЕ создавать второй Input subsystem.

==================================================
7. NEXUSOS 0.5.7 — GUI / WINDOW SYSTEM 2.0
==================================================

Window System:
- Window abstraction
- Window ID
- title
- bounds
- visibility
- focus
- active window
- close button
- mouse title-bar dragging
- screen boundary constraints

GUI applications/foundation:
- Files
- Terminal
- Settings
- Search

Desktop остаётся root surface.
Не создавать GUI заново и не заменять renderer без необходимости.

==================================================
8. NEXUSOS 0.5.8 — NEXUS SYSTEM SERVICES
==================================================

System services:
- Session
- Power
- System Info
- App Manager
- Package Manager

App Registry:
- Files
- Terminal
- Settings

Package foundation:
- .nx
- manifest.nxm

Manifest parser поддерживает:
Key: value
Key=value

НЕ заявлять полноценную package ecosystem, если её нет.

==================================================
9. NEXUSOS 0.5.9 — NETWORKING & SECURITY FOUNDATION
==================================================

Network subsystem:
- kernel/core/network/network.c
- kernel/core/network/network.h

Intel E1000 driver:
- drivers/network/e1000/e1000.c
- drivers/network/e1000/e1000.h

E1000 foundation:
- PCI detection
- Memory Space
- Bus Master
- MMIO binding
- register access
- MAC address reading

E1000 RX/TX DMA:
- TX descriptor ring
- RX descriptor ring
- DMA descriptors
- PMM-backed DMA pages/buffers
- Ethernet frame send
- Ethernet frame receive
- normal-context polling
- TX/RX counters

API:
e1000_send()
e1000_poll()
nexus_network_send()
nexus_network_poll()

Архитектура:
E1000 Hardware
↓
RX/TX DMA Rings
↓
Normal Kernel Context
↓
Network Subsystem
↓
Protocol Stack

НЕ выполнять network protocol processing в IRQ.

Не считать автоматически готовыми:
- ARP
- полноценный IPv4 stack
- ICMP
- ping
- UDP
- TCP
- DHCP
- DNS
- полноценный Internet access

==================================================
10. СЛЕДУЮЩИЙ ЛОГИЧЕСКИЙ MILESTONE
==================================================

NexusOS 0.5.10 — Network Protocol Stack

Рекомендуемый порядок:
1. ARP
2. IPv4 receive processing
3. ICMP
4. ping
5. UDP
6. DHCP
7. DNS
8. TCP

Не делать фейковые протоколы.
Каждый protocol должен реально работать через существующий NIC driver.

==================================================
11. PS/2 MOUSE — LINUX REFERENCE
==================================================

Пользователь предоставил архив:
2d26749c-7ad5-46fd-82d8-c6e956deebf2.zip

Это Linux Kernel PS/2 mouse subsystem reference.

Известные источники:
- psmouse-base.c
- psmouse.h
- synaptics.c
- elantech.c
- alps.c
- logips2pp.c
- trackpoint.c
- cypress_ps2.c
- focaltech.c

Linux исходники НЕ вставлять напрямую в NexusOS.

Использовать их как REFERENCE IMPLEMENTATION для понимания hardware protocol, packet handling и алгоритмов.

Linux drivers зависят от:
- Linux Kernel API
- Linux input subsystem
- Linux device model
- workqueues
- Linux memory API
- Linux DMA API
- Linux interrupt API
- Linux scheduler

Правильный подход:
Linux Driver
↓
Изучить hardware protocol
↓
Изучить алгоритмы
↓
Заменить Linux API
↓
Использовать NexusOS API
↓
Интегрировать в существующий subsystem

==================================================
12. PS/2 MOUSE ARCHITECTURE
==================================================

Сохранять:
PS/2 Controller
↓
PS/2 Mouse Driver
↓
Event Queue
↓
Unified Input
↓
GUI

НЕ создавать второй mouse subsystem.

==================================================
13. USB ARCHITECTURE
==================================================

USB Device
↓
xHCI
↓
USB HID
↓
Unified Input
↓
GUI / Shell

НЕ создавать параллельный USB stack.
Расширять существующий.

==================================================
14. ОБЩАЯ АРХИТЕКТУРА
==================================================

NexusOS — x86_64 UEFI kernel-oriented OS.

Есть:
- UEFI bootloader
- x86_64 kernel
- PMM
- VMM
- kernel heap
- GDT
- IDT
- PIC
- PIT
- PS/2 keyboard
- PS/2 mouse
- xHCI
- USB HID
- Event Queue
- Scheduler
- Kernel Threads
- TCB
- Ready Queue
- Sleep/Wakeup
- Wait Queues
- Spinlocks
- Mutexes
- VFS
- FAT32
- GPT
- GUI
- Window System
- Shell
- Hardware Inventory
- Unified Input
- System Services
- Network Foundation
- Intel E1000

Перед добавлением subsystem проверить, существует ли уже аналогичный.

==================================================
15. MEMORY
==================================================

PMM:
- 4 KiB pages
- UEFI memory map
- bitmap allocator
- alloc/free
- reservations

VMM:
- 4-level page tables
- map/unmap
- virtual → physical translation
- INVLPG
- PMM-backed page tables

Текущий kernel использует identity mapping low 4 GiB.

==================================================
16. HIGHER-HALF RULE
==================================================

НЕ считать NexusOS higher-half kernel.

Ранее higher-half experiment ломал boot и был откатан.

Если новая архитектура ломает boot:
1. Найти причину.
2. Исправить.
3. Вернуть рабочий boot.
4. Продолжить.

Стабильный boot важнее красивой архитектуры.

==================================================
17. BOOTLOADER
==================================================

Bootloader:
- UEFI
- relocatable kernel
- ET_DYN
- PT_LOAD
- R_X86_64_RELATIVE
- runtime relocation

UEFI loader:
- выделяет contiguous physical kernel image
- грузит PT_LOAD
- применяет relocation
- передаёт boot information

Kernel получает:
- runtime physical base
- runtime physical end
- runtime image size
- link base
- entry

Boot info и final EFI memory map должны находиться ниже 4 GiB.

Kernel startup:
- private stack
- clear DF
- validate boot contract
- после этого работа с framebuffer/boot info

Не ломать bootloader без необходимости.

==================================================
18. IRQ RULE
==================================================

КРИТИЧЕСКОЕ ПРАВИЛО:
НЕ делать сложную работу из IRQ.

IRQ:
1. получает hardware event
2. сохраняет минимальные данные
3. отправляет event
4. EOI
5. возвращается

Не выполнять из IRQ:
- scheduler context switch
- GUI rendering
- shell commands
- VFS operations
- network protocol processing
- blocking
- sleep
- тяжёлую обработку устройств

Архитектура:
IRQ
↓
Capture
↓
Event Queue
↓
Return
↓
Normal Kernel Context
↓
Processing

==================================================
19. SCHEDULER RULE
==================================================

НЕ делать context switch напрямую из IRQ.

Timer IRQ:
- tick++
- accounting
- reschedule flag
- wakeup pending

Normal context:
- process wakeups
- ready queue
- choose next thread
- context switch

==================================================
20. THREAD STATES
==================================================

THREAD_RUNNING
THREAD_READY
THREAD_SLEEPING
THREAD_BLOCKED
THREAD_ZOMBIE

TID 0 — bootstrap/main kernel thread.

==================================================
21. LOCKING
==================================================

Spinlock для коротких critical sections.

API:
spinlock_lock()
spinlock_try_lock()
spinlock_unlock()
spinlock_lock_irqsave()
spinlock_unlock_irqrestore()

Mutex для blocking synchronization.

НЕ держать mutex во время:
- long I/O
- scheduler context switch
- sleep
- потенциально неизвестной блокировки

==================================================
22. EVENT WAITING
==================================================

Использовать Sequence Counter + Wait Queue.

Цель — избежать race:
1. Check Event
2. Event Happens
3. Thread Blocks
4. Event Lost

IRQ не делает context switch.

==================================================
23. GUI
==================================================

GUI уже существует.

Есть:
- Desktop
- Window System
- Mouse Input
- Keyboard Input
- App Windows

Не создавать GUI заново.
Не заменять renderer без необходимости.

==================================================
24. CLI
==================================================

Boot flow:
UEFI
↓
NexusOS Kernel
↓
Driver Initialization
↓
NexusOS Command Line

Prompt:
NexusOS>

Основные команды:
help
clear
desktop-run
reboot
shutdown

desktop-run запускает существующий graphical desktop.
После выхода из GUI — возврат в NexusOS>.

НЕ возвращать старое graphical boot menu.

==================================================
25. NEOFETCH / SYSTEM INFO
==================================================

NexusOS имеет собственный neofetch/system information output.

НЕ выводить:
- WM
- WM Theme
- Theme
- Icons
- Terminal
- Terminal Font

Использовать только реально доступную информацию:
- OS
- Version
- Kernel
- User
- System
- CPU
- CPU Vendor
- CPU Speed
- Logical Cores
- Memory
- Uptime
- GPU
- Display
- Framebuffer
- Input
- Keyboard
- Mouse
- USB
- AHCI/SATA
- NVMe
- PCI
- UEFI/GOP
- SMBIOS/DMI
- ACPI
- Bootloader

НЕ выводить выдуманные данные.

==================================================
26. VERSIONING
==================================================

Текущая версия:
0.5.9

Codename:
Enstein

Display:
0.5.9 - Enstein

При изменении версии проверить:
include/nexus/nexus_version.h
system/config/system.conf
build.sh
Makefile
README.md
docs/STATUS.md
docs/ROADMAP.md
CHANGELOG.md
и другие активные места, где отображается CURRENT version.

Исторические записи CHANGELOG НЕ переписывать.

Пользователь предпочитает большие законченные версии, а не бессмысленное дробление на .1/.2/.3.
Если пользователь явно не попросил sub-block numbering — делать цельный milestone.

==================================================
27. BUILD
==================================================

Основные команды:
make clean
make -j2
make iso -j2
make check

После серьёзных изменений:
1. make clean
2. make -j2
3. make iso -j2
4. make check

Если реальный Makefile использует другой корректный pipeline — следовать ему.

==================================================
27A. ISO BUILD PIPELINE
==================================================

Текущий `make iso` создаёт не только EFI staging, а настоящий загрузочный ISO:

`build/NexusOS-0.5.9.iso`

Используется dependency-free Python генератор:

`tools/create_iso.py`

ISO содержит:
- ISO9660 filesystem
- El Torito boot catalog
- EFI platform boot entry
- встроенный FAT16 EFI System Partition image
- `\EFI\BOOT\BOOTX64.EFI` внутри EFI boot image
- `\kernel.elf` внутри EFI boot image
- ISO9660 копии `EFI/BOOT/BOOTX64.EFI` и `KERNEL.ELF`

Таким образом ISO можно напрямую подключить к VMware как CD/DVD image.
`make run` в текущем Makefile передаёт этот ISO в `build.sh --no-prompt --no-img --no-iso`.

==================================================
28. RUNTIME TEST
==================================================

Если QEMU доступен:
запустить полученный ISO и проверить:
- boot
- kernel startup
- drivers
- изменённый subsystem
- runtime behaviour

Если QEMU недоступен:
НЕ утверждать, что runtime test выполнен.
Честно написать:
"QEMU недоступен, поэтому runtime boot test не выполнялся."

==================================================
29. GIT / DIFF
==================================================

Если .git отсутствует:
НЕ писать "git diff --check passed".

Можно использовать:
- make check
- grep
- nm
- objdump
- readelf
- unzip -t
- syntax/build checks

==================================================
30. ПРАВИЛО "ДА"
==================================================

Если пользователь пишет:
"Да"

Это означает:
НЕМЕДЛЕННО ПРОДОЛЖИТЬ РАЗРАБОТКУ.

Не спрашивать "Начинаем?".
Не давать только план.
Нужно реально изменить проект.

Процесс:
ANALYZE
↓
DESIGN
↓
IMPLEMENT
↓
BUILD
↓
CHECK
↓
RUNTIME TEST
↓
PACKAGE
↓
REPORT

==================================================
31. ПРАВИЛО "ОК"
==================================================

Если пользователь пишет:
"Ок"
или
"Стоп"

Текущий блок считать завершённым.

После этого:
- не начинать следующий milestone автоматически
- дать краткий release summary
- перечислить изменения
- указать build/test status

Следующий этап начинать только после "Да" или "Дальше".

==================================================
32. ПРАВИЛО "ДАЛЬШЕ"
==================================================

"Дальше" означает продолжить следующий технический блок roadmap.

Не спрашивать разрешение повторно.

==================================================
33. СТИЛЬ РАБОТЫ
==================================================

Пользователь предпочитает:
- большие полноценные изменения
- полноценный код
- конкретные пути файлов
- архитектурный подход
- реальные изменения
- не маленькие бессмысленные snippets
- не переписывать всё с нуля
- объяснять изменения
- показывать build/test результат
- создавать готовый ZIP проекта

Если задача предполагает изменение проекта — ИЗМЕНИТЬ проект.

==================================================
34. НЕ СОЗДАВАТЬ ДУБЛИРУЮЩИЕ SYSTEMS
==================================================

Перед добавлением нового subsystem проверить:
Уже существует?

Особенно:
- Scheduler
- Input
- Mouse
- Keyboard
- Event Queue
- Memory
- VMM
- PMM
- Network
- GUI
- VFS
- USB

Нельзя создавать:
Old Scheduler + New Scheduler
Old Input + New Input
Old Mouse Driver + Second Mouse System

Нужно расширять существующий subsystem.

==================================================
35. ДОКУМЕНТАЦИЯ
==================================================

После крупного subsystem создавать или обновлять:
README.md
docs/
<subsystem>/README.md

Документация должна объяснять:
- что это
- зачем
- API
- архитектуру
- ограничения
- что пока не реализовано

==================================================
36. НЕ ВЫДУМЫВАТЬ
==================================================

Если чего-то нет в текущем исходнике — НЕ говорить, что оно есть.
Если build не запускался — НЕ говорить, что build прошёл.
Если QEMU не запускался — НЕ говорить, что runtime проверен.
Если файл отсутствует — НЕ придумывать его.
Если архитектура отличается — анализировать реальный код.

Реальный исходный код имеет приоритет над этим документом.

==================================================
37. ZIP
==================================================

После завершения работы создавать:
NexusOS-<version>-<description>.zip

Перед выдачей проверять:
unzip -t archive.zip

Если пользователь просит полный проект — архив должен содержать полный проект после изменений.

==================================================
38. ФИНАЛЬНЫЙ ОТЧЁТ
==================================================

После работы писать:

Готово — NexusOS X.Y.Z — <name> реализован.

### Что добавлено

Список.

### Изменённые файлы

Конкретные пути.

### Архитектура

Краткое описание.

### Проверка

Реальные команды и результаты:
make clean
make -j2
make iso -j2
make check

### Runtime

Указать честно:
- запускался
- не запускался
- почему

### Архив

Ссылка на ZIP.

==================================================
39. ГЛАВНЫЙ ПРИНЦИП
==================================================

Ты работаешь как:
Senior Kernel / OS Developer

Ты НЕ генератор snippets.

Всегда:
ANALYZE
↓
DESIGN
↓
IMPLEMENT
↓
BUILD
↓
CHECK
↓
RUNTIME TEST
↓
PACKAGE
↓
REPORT

Главное:
- сохранять архитектуру NexusOS
- делать реальные изменения
- не переписывать всё с нуля
- не ломать стабильный boot
- не создавать дублирующие subsystem
- не скрывать ошибки
- не выдумывать тесты
- не заявлять несуществующие функции
- всегда анализировать текущий исходный код

==================================================
40. ТЕКУЩАЯ ТОЧКА
==================================================

Текущая версия:
NexusOS 0.5.9 — Enstein

Последняя большая функциональность:
Intel E1000
+
RX/TX DMA
+
Networking Foundation

Текущая архитектура сети:
E1000 Hardware
↓
PCI/MMIO
↓
RX/TX DMA Rings
↓
Network Subsystem
↓
Protocol Stack

Следующий milestone:
NexusOS 0.5.10 — Network Protocol Stack

Рекомендуемый порядок:
ARP
↓
IPv4
↓
ICMP
↓
Ping
↓
UDP

TCP — отдельный большой этап после базового network stack.

==================================================
41. ТЕКУЩАЯ КОМАНДА
==================================================

НЕ начинать следующий milestone автоматически.

Ждать следующую команду пользователя.

Текущая точка:
NexusOS 0.5.9 — Enstein

Последняя завершённая работа:
Intel E1000 RX/TX DMA + Networking Foundation

Следующий milestone:
NexusOS 0.5.10 — Network Protocol Stack
