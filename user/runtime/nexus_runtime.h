#ifndef NEXUS_USER_RUNTIME_H
#define NEXUS_USER_RUNTIME_H

#include <stdint.h>
#include "syscall_abi.h"
#include "runtime_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

long nexus_syscall6(long number, long a0, long a1, long a2, long a3, long a4, long a5);
long nexus_runtime_info(nexus_runtime_info_t *out);
long nexus_channel_create(uint64_t peer_pid);
long nexus_channel_close(uint32_t handle);
long nexus_channel_send(uint32_t handle, const void *data, uint32_t size);
long nexus_channel_recv(uint32_t handle, void *data, uint32_t capacity, uint32_t *out_size);
long nexus_channel_poll(uint32_t handle);
long nexus_capabilities(uint64_t *out_mask);
long nexus_has_capability(uint64_t capability);
long nexus_drop_capability(uint64_t capability);

#ifdef __cplusplus
}
#endif

#endif
