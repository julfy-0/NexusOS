#include "modules.h"
#include "console.h"
#include "module.h"

void modules_run(void) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("Loaded modules\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    for (int i = 0; i < module_count(); ++i) {
        const nexus_module_runtime_t *r = 0;
        /* module storage is intentionally private; probe a fixed list of the
         * built-in module names so the shell only depends on the public API. */
        static const char *names[] = {
            "pci","pic","pit","input","keyboard","mouse","network",
            "nvme","ahci","usb","gpu","font"
        };
        if (i >= (int)(sizeof(names)/sizeof(names[0]))) break;
        r = module_find(names[i]);
        if (!r || !r->descriptor) continue;
        console_print("  ");
        console_print(r->descriptor->name);
        console_print(" ");
        console_print(r->descriptor->version ? r->descriptor->version : "unknown");
        console_print(" [ ");
        console_set_color(r->state == NEXUS_MODULE_STATE_LOADED ? COLOR_GREEN : COLOR_RED, COLOR_BLACK);
        console_print(r->state == NEXUS_MODULE_STATE_LOADED ? " OK  " : " FAIL ");
        console_set_color(COLOR_WHITE, COLOR_BLACK);
        console_print("]\n");
    }
    console_print("  loaded: ");
    console_print_dec(module_loaded_count());
    console_print("  failed: ");
    console_print_dec(module_failed_count());
    console_print("\n");
}
