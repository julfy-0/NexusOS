#ifndef NEXUSOS_GDT_H
#define NEXUSOS_GDT_H
#include <stdint.h>
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA   0x1B
#define GDT_USER_CODE   0x23
void gdt_init(void);
void gdt_set_kernel_stack(uint64_t rsp0);
int gdt_usermode_ready(void);
#endif
