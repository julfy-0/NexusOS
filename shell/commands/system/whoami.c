#include "whoami.h"
#include "console.h"

void whoami_run(void) {
    /* Пока нет пользователей/логина — система однопользовательская,
     * работает всегда от "root" по аналогии с ранними Unix. */
    console_print("root (NexusOS has no user accounts yet)\n");
}
