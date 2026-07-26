/* NexusOS: точка входа ядра.
 * Бутлоадер прыгает сюда с указателем на nexus_boot_info_t в RDI
 * (System V ABI — обычное соглашение GCC на Linux, без ms_abi). */
.section .bss
.align 16
stack_bottom:
.skip 65536   /* 64 KB стека ядра — своего, не того что был у бутлоадера */
stack_top:

.section .text
.global _start
.extern kmain

_start:
    lea stack_top(%rip), %rsp
    /* rdi (указатель на boot_info) не трогаем — он уже на месте для kmain */
    call kmain

.hang:
    cli
    hlt
    jmp .hang

.section .note.GNU-stack,"",@progbits
