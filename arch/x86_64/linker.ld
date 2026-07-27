/* NexusOS kernel linker script.
 *
 * Identity mapping (виртуальный адрес == физический) — но с Milestone 0.4
 * это уже СВОИ page tables (mm/paging.c, docs/adr/0003), не унаследованные
 * от UEFI. Переход на честный higher-half — отдельный будущий шаг того же
 * милстоуна (см. ROADMAP.md), адрес 0x200000 ниже пока не меняется. */
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
