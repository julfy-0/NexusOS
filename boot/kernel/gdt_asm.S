/* NexusOS: lgdt + перезагрузка CS требует far jump/return — это нельзя
 * выразить на чистом C. */
.section .text
.global gdt_flush
.type gdt_flush, @function

# rdi = адрес struct gdt_ptr { limit:16; base:64 } (packed)
gdt_flush:
    lgdt (%rdi)

    mov $0x10, %ax      # селектор данных
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs

    # Перезагружаем CS через far return на следующую инструкцию
    pushq $0x08          # селектор кода
    lea .reload_cs(%rip), %rax
    pushq %rax
    lretq
.reload_cs:
    ret

.section .note.GNU-stack,"",@progbits
