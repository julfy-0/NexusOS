#ifndef NEXUSOS_USERMODE_H
#define NEXUSOS_USERMODE_H
#include <stdint.h>
/* Milestone 1 foundation: ring-3 descriptors, TSS kernel stack and process ABI. */
typedef struct { uint64_t entry, user_stack, kernel_stack; } nexus_user_process_t;
void usermode_init(void);
int usermode_ready(void);
#endif
