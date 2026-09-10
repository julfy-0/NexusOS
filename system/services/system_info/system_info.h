#ifndef NEXUSOS_SYSTEM_INFO_H
#define NEXUSOS_SYSTEM_INFO_H
#include <stdint.h>
int nexus_system_info_init(void);
const char *nexus_system_name(void);
uint32_t nexus_system_major(void);
uint32_t nexus_system_minor(void);
uint32_t nexus_system_patch(void);
#endif
