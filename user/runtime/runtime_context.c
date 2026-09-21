#include "runtime_context.h"
#include "nexus_runtime.h"

static void runtime_context_zero(nexus_runtime_context_t *ctx) {
    if (!ctx) return;
    unsigned char *p = (unsigned char *)ctx;
    for (uint32_t i = 0; i < (uint32_t)sizeof(*ctx); ++i) p[i] = 0;
}

long nexus_runtime_context_open(nexus_runtime_context_t *ctx) {
    if (!ctx) return -1;
    runtime_context_zero(ctx);
    return nexus_runtime_context_refresh(ctx);
}

long nexus_runtime_context_refresh(nexus_runtime_context_t *ctx) {
    if (!ctx) return -1;
    nexus_runtime_info_t info;
    long rc = nexus_runtime_info(&info);
    if (rc != 0) {
        ctx->valid = 0;
        return rc;
    }
    ctx->info = info;
    ctx->valid = 1;
    return 0;
}

int nexus_runtime_context_valid(const nexus_runtime_context_t *ctx) {
    return ctx && ctx->valid && ctx->info.abi_version != 0;
}

uint64_t nexus_runtime_pid(const nexus_runtime_context_t *ctx) {
    return nexus_runtime_context_valid(ctx) ? ctx->info.process_pid : 0;
}

uint64_t nexus_runtime_parent_pid(const nexus_runtime_context_t *ctx) {
    return nexus_runtime_context_valid(ctx) ? ctx->info.process_parent_pid : 0;
}

uint64_t nexus_runtime_address_space(const nexus_runtime_context_t *ctx) {
    return nexus_runtime_context_valid(ctx) ? ctx->info.process_address_space : 0;
}

uint64_t nexus_runtime_features(const nexus_runtime_context_t *ctx) {
    return nexus_runtime_context_valid(ctx) ? ctx->info.feature_bits : 0;
}

uint32_t nexus_runtime_abi(const nexus_runtime_context_t *ctx) {
    return nexus_runtime_context_valid(ctx) ? ctx->info.abi_version : 0;
}

uint32_t nexus_runtime_syscall_abi(const nexus_runtime_context_t *ctx) {
    return nexus_runtime_context_valid(ctx) ? ctx->info.syscall_abi_version : 0;
}

uint32_t nexus_runtime_channel_limit(const nexus_runtime_context_t *ctx) {
    return nexus_runtime_context_valid(ctx) ? ctx->info.channel_limit : 0;
}

uint32_t nexus_runtime_message_size(const nexus_runtime_context_t *ctx) {
    return nexus_runtime_context_valid(ctx) ? ctx->info.message_size : 0;
}

int nexus_runtime_has_feature(const nexus_runtime_context_t *ctx, uint64_t feature) {
    if (!nexus_runtime_context_valid(ctx) || feature == 0) return 0;
    return (ctx->info.feature_bits & feature) == feature;
}

const char *nexus_runtime_name_from_context(const nexus_runtime_context_t *ctx) {
    return nexus_runtime_context_valid(ctx) ? ctx->info.name : "";
}
