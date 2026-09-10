#include "gui_state.h"

static nexus_gui_context_t g_gui;

nexus_gui_context_t *gui_context(void) {
    return &g_gui;
}
