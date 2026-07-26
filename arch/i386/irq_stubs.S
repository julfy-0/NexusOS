/*
 * NexusOS — irq.S
 * Аппаратные прерывания IRQ0-15 от PIC мы перемаппим на векторы
 * IDT 32-47 (см. irq_init в irq.c), чтобы не конфликтовать с
 * исключениями CPU (0-31). Структура стабов идентична isr.S.
 */

.macro IRQ num, remapped
.global irq\num
irq\num:
    cli
    push $0
    push $\remapped
    jmp irq_common_stub
.endm

IRQ 0,  32
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

.extern irq_handler
.global irq_common_stub
irq_common_stub:
    push %ds
    push %edi
    push %esi
    push %ebp
    push %esp
    push %ebx
    push %edx
    push %ecx
    push %eax

    mov $0x10, %ax
    mov %ax, %ds

    call irq_handler

    pop %eax
    pop %ecx
    pop %edx
    pop %ebx
    pop %esp
    pop %ebp
    pop %esi
    pop %edi
    pop %ds

    add $8, %esp
    sti
    iret
