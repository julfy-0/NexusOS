#include "version.h"
#include "console.h"
#include "nexus_version.h"

void version_run(void) {
    console_print("NexusOS " NEXUS_VERSION_STRING " (monolithic kernel, shell runs in kernel context)\n");
}
