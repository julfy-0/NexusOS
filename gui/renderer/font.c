#include <stdint.h>
#include "font.h"
#include "font8x16.h"

const uint8_t *gui_font_glyph(char c) {
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) return 0;
    return font8x16[(uint8_t)c - FONT_FIRST_CHAR];
}
