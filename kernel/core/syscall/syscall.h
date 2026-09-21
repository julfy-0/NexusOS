#ifndef NEXUSOS_SYSCALL_H
#define NEXUSOS_SYSCALL_H
#include <stdint.h>

#include "syscall_abi.h"


void syscall_init(void);
int syscall_ready(void);
uint64_t syscall_dispatch(uint64_t number, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3);

#define NEXUS_SYSCALL_ACTION_RETURN 0ULL
#define NEXUS_SYSCALL_ACTION_EXIT   1ULL

uint64_t syscall_interrupt_handler(uint64_t *frame);

#endif
