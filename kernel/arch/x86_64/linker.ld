/* NexusOS kernel image layout.
 *
 * The kernel is linked at a deterministic virtual/physical image base, but
 * the UEFI loader treats the resulting ET_DYN image as relocatable. This
 * gives us a stable ABI for the kernel while allowing the loader to place
 * the image in any suitable physical RAM below 4 GiB.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x200000;
    __kernel_start = .;

    .text : ALIGN(4096) {
        *(.text .text.*)
    }

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
    }

    .nexus_modules : ALIGN(8) {
        __nexus_modules_start = .;
        KEEP(*(.nexus_modules))
        __nexus_modules_end = .;
    }

    .data : ALIGN(4096) {
        *(.data .data.*)
    }

    .bss : ALIGN(4096) {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        __bss_end = .;
    }

    __kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }
}
