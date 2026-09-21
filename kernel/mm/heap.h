#ifndef NEXUSOS_KERNEL_HEAP_H
#define NEXUSOS_KERNEL_HEAP_H

#include <stddef.h>
#include <stdint.h>

/* NexusOS kernel heap.
 *
 * The heap lives in a dedicated high virtual-address arena. Physical pages
 * are obtained from PMM and mapped through the VMM on demand. The allocator
 * uses a first-fit free-list with block splitting/coalescing.
 */
void heap_init(void);

void *kmalloc(size_t size);
void kfree(void *ptr);

int heap_is_ready(void);
uint64_t heap_virtual_base(void);
uint64_t heap_virtual_end(void);
uint64_t heap_mapped_pages(void);
uint64_t heap_used_bytes(void);
uint64_t heap_free_bytes(void);
uint64_t heap_allocations(void);
int heap_validate(void);
uint64_t heap_corruption_count(void);

#endif
