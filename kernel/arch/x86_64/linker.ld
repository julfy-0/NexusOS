/* NexusOS higher-half kernel linker layout.
 *
 * The tiny .text.entry trampoline is linked at its physical load address so
 * the UEFI loader can enter it before our page tables exist. The real kernel
 * is linked at the canonical higher-half base and loaded into contiguous
 * physical memory beginning at 2 MiB. The bootstrap trampoline creates a
 * temporary high alias, then jumps to kernel_high_entry.
 */
ENTRY(_start)

KERNEL_PHYS_BASE = 0x200000;
KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;

SECTIONS
{
    .text.entry KERNEL_PHYS_BASE : AT(KERNEL_PHYS_BASE) ALIGN(4096)
    {
        KEEP(*(.text.entry))
    }

    .text KERNEL_VIRT_BASE : AT(ALIGN(KERNEL_PHYS_BASE + SIZEOF(.text.entry), 4096))
    {
        __kernel_virtual_start = .;
        *(.text .text.*)
    }

    .rodata ALIGN(4096) : AT(ALIGN(LOADADDR(.text) + SIZEOF(.text), 4096))
    {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4096) : AT(ALIGN(LOADADDR(.rodata) + SIZEOF(.rodata), 4096))
    {
        *(.data .data.*)
    }

    .bss.bootstrap ALIGN(4096) (NOLOAD) : AT(ALIGN(LOADADDR(.data) + SIZEOF(.data), 4096))
    {
        __bootstrap_bss_start = .;
        *(.bss.bootstrap)
        __bootstrap_bss_end = .;
    }

    .bss ALIGN(4096) (NOLOAD) : AT(ALIGN(LOADADDR(.bss.bootstrap) + SIZEOF(.bss.bootstrap), 4096))
    {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        __bss_end = .;
    }

    __kernel_virtual_end = .;

    /* Physical symbols are used by PMM and by the early entry trampoline.
     * They deliberately do not use the higher-half VMA. */
    __kernel_phys_start = KERNEL_PHYS_BASE;
    __kernel_phys_end = LOADADDR(.bss) + SIZEOF(.bss);

    __bootstrap_pml4_phys = LOADADDR(.bss.bootstrap);
    __bootstrap_pdpt_low_phys = __bootstrap_pml4_phys + 0x1000;
    __bootstrap_pd_low_phys = __bootstrap_pdpt_low_phys + 0x1000;
    __bootstrap_pdpt_high_phys = __bootstrap_pd_low_phys + (4 * 0x1000);
    __bootstrap_pd_high_phys = __bootstrap_pdpt_high_phys + 0x1000;

    __stack_top_phys = LOADADDR(.bss) + (stack_top - ADDR(.bss));

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }
}
