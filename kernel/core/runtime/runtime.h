#ifndef NEXUSOS_RUNTIME_H
#define NEXUSOS_RUNTIME_H

#include <stdint.h>
#include "runtime_abi.h"

int nexus_runtime_init(void);
int nexus_runtime_ready(void);
int nexus_runtime_fill_info(uint64_t pid, nexus_runtime_info_t *out);
const char *nexus_runtime_name(void);
uint32_t nexus_runtime_abi_version(void);

#endif
