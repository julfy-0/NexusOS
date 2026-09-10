#ifndef NEXUSOS_GUI_FONT_H
#define NEXUSOS_GUI_FONT_H

#include <stdint.h>

#define GUI_FONT_WIDTH  8
#define GUI_FONT_HEIGHT 16

/* Returns the bitmap rows for a supported glyph, or NULL when unavailable. */
const uint8_t *gui_font_glyph(char c);

#endif
