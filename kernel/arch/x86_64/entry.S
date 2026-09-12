/* NexusOS x86_64 kernel entry.
 *
 * The UEFI loader enters this function with RDI = nexus_boot_info_t* and
 * interrupts disabled. No firmware stack is trusted after hand-off; the
 * kernel switches to its own statically allocated stack before touching C.
 */
.section .bss
.align 16
stack_bottom:
.skip 65536
stack_top:

.section .text
.global _start
.type _start, @function
.extern kmain

_start:
    lea stack_top(%rip), %rsp
    andq $-16, %rsp
    cld
    /* Preserve the boot-info pointer in the ABI-required first argument. */
    call kmain

.hang:
    cli
    hlt
    jmp .hang

.size _start, .-_start
.section .note.GNU-stack,"",@progbits
