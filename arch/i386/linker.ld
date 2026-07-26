/*
 * NexusOS — скрипт линковщика для i386.
 * Ядро грузится по стандартному для hobby-OS адресу 1 MiB —
 * ниже этой границы память фрагментирована legacy-регионами BIOS/VGA.
 */
ENTRY(_start)

SECTIONS
{
    . = 1M;

    kernel_start = .;

    .multiboot ALIGN(4K) : {
        *(.multiboot)
    }

    .text ALIGN(4K) : {
        *(.text)
    }

    .rodata ALIGN(4K) : {
        *(.rodata*)
    }

    .data ALIGN(4K) : {
        *(.data)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss)
    }

    kernel_end = .;
}
