/* NexusOS kernel linker script.
 *
 * Пока используем identity mapping, оставленный прошивкой UEFI: виртуальный
 * адрес == физический. Переход на честный higher-half + свои page tables —
 * отдельный милстоун. */
ENTRY(_start)

SECTIONS
{
    . = 0x200000; /* 2 MiB — с запасом от низкой памяти и кода bootloader'а */

    .text : ALIGN(4096) {
        *(.text .text.*)
    }

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
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

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }
}
