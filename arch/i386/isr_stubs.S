/*
 * NexusOS — isr.S
 *
 * Аппаратные исключения CPU приходят прямо на конкретный адрес,
 * поэтому для каждого вектора нужен отдельный крошечный стаб,
 * который просто кладёт номер прерывания в стек и прыгает в общий
 * обработчик isr_common_stub. Это единственное место в проекте,
 * где обычный C бессилен — переключение контекста прерывания
 * требует точного контроля над стеком.
 */

.macro ISR_NOERR num
.global isr\num
isr\num:
    cli
    push $0            /* фиктивный err_code, чтобы кадр был одинаковым */
    push $\num
    jmp isr_common_stub
.endm

.macro ISR_ERR num
.global isr\num
isr\num:
    cli
    push $\num         /* err_code уже положен процессором, номер после */
    jmp isr_common_stub
.endm

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

.extern isr_handler
.global isr_common_stub
isr_common_stub:
    push %ds
    push %edi
    push %esi
    push %ebp
    push %esp
    push %ebx
    push %edx
    push %ecx
    push %eax

    mov $0x10, %ax     /* GDT_KERNEL_DATA_SEL */
    mov %ax, %ds

    call isr_handler

    pop %eax
    pop %ecx
    pop %edx
    pop %ebx
    pop %esp
    pop %ebp
    pop %esi
    pop %edi
    pop %ds

    add $8, %esp        /* снимаем int_no и err_code */
    sti
    iret
