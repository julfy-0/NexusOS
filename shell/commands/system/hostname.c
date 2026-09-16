#include "hostname.h"
#include "console.h"
#include "target.h"
void hostname_run(void) { console_print(target_baseboard_manufacturer()); console_print("\n"); }
