/* NexusOS x86_64 entry/trampoline.
 *
 * _start lives at the physical load address (2 MiB). It is intentionally
 * tiny and position-independent because the higher-half C kernel cannot be
 * executed until paging provides its virtual alias.
 *
 * Bootstrap mappings:
 *   identity:  0 .. 4 GiB
 *   higher:    FFFFFFFF80000000 .. FFFFFFFF80000000 + 4 GiB
 *
 * The normal kernel entry then executes from the higher-half alias. The
 * identity map remains active so the firmware-owned boot_info pointer and
 * low physical/MMIO addresses remain usable during early initialization.
 */
.section .text.entry,"ax"
.global _start
.type _start,@function
.extern kernel_high_entry
.extern __bootstrap_pml4_phys
.extern __bootstrap_pdpt_low_phys
.extern __bootstrap_pd_low_phys
.extern __bootstrap_pdpt_high_phys
.extern __bootstrap_pd_high_phys
.extern __stack_top_phys

_start:
    /* rdi = nexus_boot_info_t*, supplied by the UEFI loader. */
    mov %rdi, %r12

    /* Clear bootstrap PML4/PDPTs/PDs. */
    xor %eax, %eax
    mov $0, %ecx
    lea __bootstrap_pml4_phys, %rdi
    mov $512, %ecx
    mov %rdi, %r8
    rep stosq

    lea __bootstrap_pdpt_low_phys, %rdi
    mov $512, %ecx
    rep stosq

    lea __bootstrap_pdpt_high_phys, %rdi
    mov $512, %ecx
    rep stosq

    lea __bootstrap_pd_low_phys, %rdi
    mov $(4 * 512), %ecx
    rep stosq

    lea __bootstrap_pd_high_phys, %rdi
    mov $(4 * 512), %ecx
    rep stosq

    /* PML4[0] -> low PDPT, PML4[511] -> high PDPT. */
    mov $__bootstrap_pdpt_low_phys, %rax
    or $0x3, %rax
    mov %rax, __bootstrap_pml4_phys

    mov $__bootstrap_pdpt_high_phys, %rax
    or $0x3, %rax
    mov %rax, __bootstrap_pml4_phys + (511 * 8)

    /* Four 1-GiB slots, each backed by a 2-MiB PD table: identity map. */
    xor %ecx, %ecx
1:
    mov $__bootstrap_pd_low_phys, %rax
    mov %rcx, %r9
    shl $12, %r9
    add %r9, %rax
    or $0x3, %rax
    mov %rax, __bootstrap_pdpt_low_phys(,%rcx,8)
    inc %ecx
    cmp $4, %ecx
    jb 1b

    /* High alias starts at FFFFFFFF80000000: PML4[511], PDPT[510]. */
    mov $__bootstrap_pd_high_phys, %rax
    or $0x3, %rax
    mov %rax, __bootstrap_pdpt_high_phys + (510 * 8)

    /* Fill low/high PDs with 2-MiB identity/alias mappings. */
    xor %ecx, %ecx
2:
    mov $__bootstrap_pd_low_phys, %rax
    mov %rcx, %r9
    shl $12, %r9
    add %r9, %rax
    mov %rax, %r8
    xor %edx, %edx
3:
    mov %ecx, %eax
    shl $30, %rax
    mov %rdx, %r9
    shl $21, %r9
    add %r9, %rax
    or $0x83, %rax              /* present | writable | huge */
    mov %rax, (%r8,%rdx,8)

    mov $__bootstrap_pd_high_phys, %r10
    mov %rcx, %r9
    shl $12, %r9
    add %r9, %r10
    mov %rax, (%r10,%rdx,8)

    inc %edx
    cmp $512, %edx
    jb 3b
    inc %ecx
    cmp $4, %ecx
    jb 2b

    /* Install bootstrap CR3. */
    mov $__bootstrap_pml4_phys, %rax
    mov %rax, %cr3

    /* Switch to a physical stack before changing RIP to the high alias. */
    mov $__stack_top_phys, %rsp

    /* Same physical bytes, now reached through the higher-half alias. */
    mov %r12, %rdi
    mov $kernel_high_entry, %rax
    jmp *%rax

.size _start, .-_start

.section .text,"ax"
.global kernel_high_entry
.type kernel_high_entry,@function
.extern kmain
kernel_high_entry:
    lea stack_top(%rip), %rsp
    call kmain

.hang:
    cli
    hlt
    jmp .hang

.section .bss.bootstrap,"aw",@nobits
.align 4096
.global bootstrap_page_tables_start
bootstrap_page_tables_start:
.global bootstrap_pml4
bootstrap_pml4:
    .skip 4096
.global bootstrap_pdpt_low
bootstrap_pdpt_low:
    .skip 4096
.global bootstrap_pd_low
bootstrap_pd_low:
    .skip (4 * 4096)
.global bootstrap_pdpt_high
bootstrap_pdpt_high:
    .skip 4096
.global bootstrap_pd_high
bootstrap_pd_high:
    .skip (4 * 4096)
.global bootstrap_page_tables_end
bootstrap_page_tables_end:

.section .bss,"aw",@nobits
.align 16
stack_bottom:
.skip 65536
.global stack_top
stack_top:

.section .note.GNU-stack,"",@progbits
