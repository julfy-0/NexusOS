#ifndef NEXUS_KERNEL_H
#define NEXUS_KERNEL_H

#include <nexus/types.h>

/* Точка входа ядра на C, вызывается из boot/boot.S.
 * magic  — должен быть равен 0x2BADB002, иначе загрузка не через Multiboot.
 * mbi_addr — физический адрес multiboot_info структуры от GRUB. */
void kernel_main(u32 magic, u32 mbi_addr);

/* Версия ОС — единое место для номера версии во всём проекте */
#define NEXUSOS_VERSION "0.1.2"
#define NEXUSOS_NAME    "NexusOS"

#endif /* NEXUS_KERNEL_H */
