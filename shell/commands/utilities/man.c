#include "man.h"
#include "console.h"
#include "command_registry.h"

void man_run(char *args) {
    if (args[0] == '\0') {
        console_print("usage: man <command>\n");
        return;
    }
    shell_command_manual(args);
}
