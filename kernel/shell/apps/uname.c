#include "uname.h"
#include "console.h"
#include "nexus_version.h"

void uname_run(void) {
    console_print("NexusOS kernel " NEXUS_VERSION_STRING " x86_64\n");
}
