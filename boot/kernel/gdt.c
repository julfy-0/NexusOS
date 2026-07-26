/* NexusOS: собственная GDT. UEFI даёт свою, но нам нужен полный контроль
 * над сегментами до того, как мы начнём настраивать userspace. */
#include <stdint.h>
#include "gdt.h"

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* Дескрипторы для 64-битного long mode: base/limit почти не используются
 * процессором (сегментация в long mode фактически не работает для
 * code/data), важны только access byte и флаги (L-бит = long mode). */
static const uint64_t gdt[3] = {
    0x0000000000000000ULL, /* 0x00: null */
    0x00AF9A000000FFFFULL, /* 0x08: kernel code, 64-bit, DPL0 */
    0x00CF92000000FFFFULL, /* 0x10: kernel data, DPL0 */
};

static struct gdt_ptr gdtp;

extern void gdt_flush(uint64_t gdt_ptr_addr);

void gdt_init(void) {
    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base = (uint64_t)(uintptr_t)gdt;
    gdt_flush((uint64_t)(uintptr_t)&gdtp);
}
