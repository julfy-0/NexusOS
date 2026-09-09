#ifndef NEXUSOS_BOOTMODE_H
#define NEXUSOS_BOOTMODE_H

#include "boot_info.h"

typedef enum {
    NEXUS_BOOT_GRAPHIC = 0,
    NEXUS_BOOT_CLI = 1
} nexus_boot_mode_t;

nexus_boot_mode_t bootmode_select(nexus_framebuffer_t *fb);

#endif
