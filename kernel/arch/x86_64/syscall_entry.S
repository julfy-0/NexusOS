/* NexusOS x86_64 user syscall entry.
 *
 * INT 0x80 arrives through an IDT interrupt gate with DPL=3. When entered
 * from CPL3 the CPU has already switched to TSS.RSP0 and pushed:
 *   RIP, CS, RFLAGS, user RSP, SS
 * We save every GPR, call the normal-context C dispatcher, restore the exact
 * register set, and IRETQ back to the user instruction after INT 0x80.
 *
 * RAX is intentionally left writable by the C handler so it carries the ABI
 * return value. All other GPRs are restored from the saved frame. */
.section .text
.global syscall_entry
.type syscall_entry,@function
syscall_entry:
    cld
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, %rdi
    call syscall_interrupt_handler
    cmpq $1, %rax
    je .syscall_exit_switched

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    /* Handler stores the return value in the saved RAX slot. */
    popq %rax
    iretq

.syscall_exit_switched:
    /* SYS_EXIT switched away through scheduler_process(). The old user
     * interrupt frame belongs to the terminated thread and must never be
     * restored or returned to. The scheduler has already selected another
     * kernel stack; execution should never reach this point on success. */
    cli
    hlt
    jmp .syscall_exit_switched
.size syscall_entry, .-syscall_entry

.section .note.GNU-stack,"",@progbits
