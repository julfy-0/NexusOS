/*
 * NexusOS — boot.S
 *
 * Единственный файл на чистом ассемблере (синтаксис GNU AS/AT&T,
 * собирается тем же gcc, без nasm). Его задачи:
 *   1. Разместить Multiboot-заголовок, чтобы GRUB узнал загружаемое ядро.
 *   2. Настроить стек ядра.
 *   3. Передать управление в kernel_main() на C.
 * Дальше вся логика ОС пишется на C.
 */

.set MB_MAGIC,    0x1BADB002
.set MB_ALIGN,    1<<0            /* модули выровнены по границе страницы */
.set MB_MEMINFO,  1<<1            /* просить memory map у GRUB */
.set MB_FLAGS,    (MB_ALIGN | MB_MEMINFO)
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.section .multiboot
.align 4
    .long MB_MAGIC
    .long MB_FLAGS
    .long MB_CHECKSUM

/* Стек ядра: 16 KiB, выровненный по 16 байт (требование System V ABI) */
.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    mov $stack_top, %esp
    mov $0, %ebp

    /* GRUB передаёт в EAX magic-число, а в EBX — указатель на
     * multiboot_info структуру. Сохраняем их как аргументы kernel_main. */
    push %ebx
    push %eax
    call kernel_main

    /* kernel_main никогда не должен вернуться, но если это случилось —
     * останавливаем CPU в бесконечном цикле с HLT. */
.hang:
    cli
    hlt
    jmp .hang

.size _start, . - _start
