#ifndef NEXUSOS_IPC_H
#define NEXUSOS_IPC_H

#include <stdint.h>

int nexus_ipc_init(void);
int nexus_ipc_ready(void);
int nexus_ipc_create(uint64_t owner_pid, uint64_t peer_pid, uint32_t *out_handle);
int nexus_ipc_close(uint64_t pid, uint32_t handle);
int nexus_ipc_send(uint64_t pid, uint32_t handle, const void *data, uint32_t size);
int nexus_ipc_recv(uint64_t pid, uint32_t handle, void *data, uint32_t capacity, uint32_t *out_size);
uint32_t nexus_ipc_poll(uint64_t pid, uint32_t handle);
void nexus_ipc_process_terminate(uint64_t pid);
uint32_t nexus_ipc_open_count(uint64_t pid);

#endif
