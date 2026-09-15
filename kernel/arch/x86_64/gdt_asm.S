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

.global gdt_load_tss
.type gdt_load_tss,@function
gdt_load_tss:
    ltr %di
    ret

/* Build the long-mode privilege transition frame exactly as IRETQ expects.
 * rdi=entry, rsi=user RSP. SS/RSP/RFLAGS/CS/RIP are popped by iretq. */
.global arch_enter_user_mode
.type arch_enter_user_mode,@function
arch_enter_user_mode:
    pushq $0x1b
    pushq %rsi
    pushfq
    popq %rax
    orq $0x200, %rax
    pushq %rax
    pushq $0x23
    pushq %rdi
    iretq
.size arch_enter_user_mode, .-arch_enter_user_mode

.section .note.GNU-stack,"",@progbits
