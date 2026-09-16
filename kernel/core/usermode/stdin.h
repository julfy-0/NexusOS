#ifndef NEXUSOS_USER_STDIN_H
#define NEXUSOS_USER_STDIN_H

#include <stdint.h>

void userspace_stdin_init(void);
int userspace_stdin_ready(void);
int userspace_stdin_bind(uint64_t pid);
void userspace_stdin_unbind(uint64_t pid);
uint64_t userspace_stdin_owner(void);
uint64_t userspace_stdin_available(uint64_t pid);
int userspace_stdin_read(uint64_t pid, void *buffer, uint64_t size, uint64_t *out_read);
int userspace_stdin_push(uint8_t c);
int userspace_stdin_wait(uint64_t pid);
uint32_t userspace_stdin_waiter_count(void);
uint64_t userspace_stdin_sequence(void);

#endif
