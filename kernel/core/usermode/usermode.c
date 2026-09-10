#include "usermode.h"
#include "gdt.h"
static int ready;
void usermode_init(void){
    /* GDT/TSS is installed before interrupts. Actual scheduler and per-process
       page tables come next; keeping this separate prevents fake "user mode". */
    ready = gdt_usermode_ready();
}
int usermode_ready(void){return ready;}
