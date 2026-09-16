#ifndef NEXUS_FONT_SERVICE_H
#define NEXUS_FONT_SERVICE_H

#include <stdint.h>

int nexus_font_service_init(void);
int nexus_font_service_ready(void);
const char *nexus_font_family(void);
uint64_t nexus_font_regular_size(void);
uint64_t nexus_font_bold_size(void);

#endif
