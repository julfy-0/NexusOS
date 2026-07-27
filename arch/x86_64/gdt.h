#ifndef NEXUSOS_GDT_H
#define NEXUSOS_GDT_H

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10

void gdt_init(void);

#endif
