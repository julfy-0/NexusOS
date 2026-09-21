#ifndef NEXUS_RUNTIME_CONTEXT_H
#define NEXUS_RUNTIME_CONTEXT_H

#include <stdint.h>
#include "runtime_abi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    nexus_runtime_info_t info;
    uint32_t valid;
} nexus_runtime_context_t;

long nexus_runtime_context_open(nexus_runtime_context_t *ctx);
long nexus_runtime_context_refresh(nexus_runtime_context_t *ctx);
int nexus_runtime_context_valid(const nexus_runtime_context_t *ctx);
uint64_t nexus_runtime_pid(const nexus_runtime_context_t *ctx);
uint64_t nexus_runtime_parent_pid(const nexus_runtime_context_t *ctx);
uint64_t nexus_runtime_address_space(const nexus_runtime_context_t *ctx);
uint64_t nexus_runtime_features(const nexus_runtime_context_t *ctx);
uint32_t nexus_runtime_abi(const nexus_runtime_context_t *ctx);
uint32_t nexus_runtime_syscall_abi(const nexus_runtime_context_t *ctx);
uint32_t nexus_runtime_channel_limit(const nexus_runtime_context_t *ctx);
uint32_t nexus_runtime_message_size(const nexus_runtime_context_t *ctx);
int nexus_runtime_has_feature(const nexus_runtime_context_t *ctx, uint64_t feature);
const char *nexus_runtime_name_from_context(const nexus_runtime_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif
