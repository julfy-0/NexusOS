#include <stdint.h>
#include "gdt.h"
struct gdt_ptr { uint16_t limit; uint64_t base; } __attribute__((packed));
struct tss64 { uint32_t r0; uint64_t rsp0,rsp1,rsp2; uint64_t r1; uint64_t ist1,ist2,ist3,ist4,ist5,ist6,ist7; uint64_t r2; uint16_t r3,iomap; } __attribute__((packed));
static uint64_t gdt[7]; static struct tss64 tss; static struct gdt_ptr gdtp;
extern void gdt_flush(uint64_t); extern void gdt_load_tss(uint16_t);
static uint64_t desc(uint8_t access,uint8_t flags){ return 0xFFFFULL | ((uint64_t)access<<40) | ((uint64_t)flags<<52); }
void gdt_init(void){
 gdt[0]=0; gdt[1]=desc(0x9A,0xA); gdt[2]=desc(0x92,0xC); gdt[3]=desc(0xF2,0xC); gdt[4]=desc(0xFA,0xA);
 uint64_t base=(uint64_t)(uintptr_t)&tss; uint64_t low=0x67ULL|((base&0xFFFFFFULL)<<16)|((uint64_t)0x89<<40)|(((base>>24)&0xFFULL)<<56); uint64_t high=base>>32; gdt[5]=low;gdt[6]=high;
 tss.iomap=sizeof(tss); gdtp.limit=sizeof(gdt)-1;gdtp.base=(uint64_t)(uintptr_t)gdt;gdt_flush((uint64_t)(uintptr_t)&gdtp);gdt_load_tss(0x28);
}
void gdt_set_kernel_stack(uint64_t rsp0){tss.rsp0=rsp0;}
int gdt_usermode_ready(void){return tss.iomap==sizeof(tss);}
