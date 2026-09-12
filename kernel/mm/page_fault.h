#ifndef NEXUSOS_PAGE_FAULT_H
#define NEXUSOS_PAGE_FAULT_H

#include <stdint.h>

struct nexus_interrupt_frame;
typedef struct nexus_interrupt_frame interrupt_frame_t;

/* Decode and print the x86_64 page-fault state, then enter the normal
 * NexusOS panic/restart path. This function does not attempt recovery. */
void page_fault_handle(const interrupt_frame_t *frame);

#endif
