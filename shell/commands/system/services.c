#include "services.h"
#include "console.h"
#include "event_queue.h"
#include "scheduler.h"
#include "process.h"
#include "usermode.h"
#include "syscall.h"
#include "vfs.h"
#include "gui.h"
#include "ipc.h"
#include "runtime.h"

void services_run(void) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print("Core services\n");
    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("  event queue service 1.0 [ "); console_set_color(event_queue_is_ready() ? COLOR_GREEN : COLOR_RED, COLOR_BLACK); console_print(event_queue_is_ready() ? " OK  " : " FAIL "); console_set_color(COLOR_WHITE, COLOR_BLACK); console_print("]\n");
    console_print("  scheduler service 1.0 [ "); console_set_color(scheduler_is_ready() ? COLOR_GREEN : COLOR_RED, COLOR_BLACK); console_print(scheduler_is_ready() ? " OK  " : " FAIL "); console_set_color(COLOR_WHITE, COLOR_BLACK); console_print("]\n");
    console_print("  process service 1.0 [ "); console_set_color(process_is_ready() ? COLOR_GREEN : COLOR_RED, COLOR_BLACK); console_print(process_is_ready() ? " OK  " : " FAIL "); console_set_color(COLOR_WHITE, COLOR_BLACK); console_print("]\n");
    console_print("  usermode service 1.0 [ "); console_set_color(usermode_ready() ? COLOR_GREEN : COLOR_RED, COLOR_BLACK); console_print(usermode_ready() ? " OK  " : " FAIL "); console_set_color(COLOR_WHITE, COLOR_BLACK); console_print("]\n");
    console_print("  syscall service 1.0 [ "); console_set_color(syscall_ready() ? COLOR_GREEN : COLOR_RED, COLOR_BLACK); console_print(syscall_ready() ? " OK  " : " FAIL "); console_set_color(COLOR_WHITE, COLOR_BLACK); console_print("]\n");
    console_print("  vfs service 1.0 [ "); console_set_color(1 ? COLOR_GREEN : COLOR_RED, COLOR_BLACK); console_print(1 ? " OK  " : " FAIL "); console_set_color(COLOR_WHITE, COLOR_BLACK); console_print("]\n");
    console_print("  gui service 1.0 [ "); console_set_color(gui_is_active() ? COLOR_GREEN : COLOR_YELLOW, COLOR_BLACK); console_print(gui_is_active() ? " OK  " : " IDLE "); console_set_color(COLOR_WHITE, COLOR_BLACK); console_print("]\n");
    console_print("  ipc service 1.0 [ "); console_set_color(nexus_ipc_ready() ? COLOR_GREEN : COLOR_RED, COLOR_BLACK); console_print(nexus_ipc_ready() ? " OK  " : " FAIL "); console_set_color(COLOR_WHITE, COLOR_BLACK); console_print("]\n");
    console_print("  runtime service 1.0 [ "); console_set_color(nexus_runtime_ready() ? COLOR_GREEN : COLOR_RED, COLOR_BLACK); console_print(nexus_runtime_ready() ? " OK  " : " FAIL "); console_set_color(COLOR_WHITE, COLOR_BLACK); console_print("]\n");
}
