#include <stdint.h>
#include "font_service.h"

extern const unsigned char _binary_assets_fonts_Roboto_Regular_ttf_start[];
extern const unsigned char _binary_assets_fonts_Roboto_Regular_ttf_end[];
extern const unsigned char _binary_assets_fonts_Roboto_Bold_ttf_start[];
extern const unsigned char _binary_assets_fonts_Roboto_Bold_ttf_end[];

static int g_ready;
static uint64_t g_regular_size;
static uint64_t g_bold_size;

int nexus_font_service_init(void) {
    g_regular_size = (uint64_t)(_binary_assets_fonts_Roboto_Regular_ttf_end -
                                _binary_assets_fonts_Roboto_Regular_ttf_start);
    g_bold_size = (uint64_t)(_binary_assets_fonts_Roboto_Bold_ttf_end -
                             _binary_assets_fonts_Roboto_Bold_ttf_start);
    g_ready = g_regular_size > 0 && g_bold_size > 0;
    return g_ready;
}

int nexus_font_service_ready(void) { return g_ready; }
const char *nexus_font_family(void) { return "Roboto"; }
uint64_t nexus_font_regular_size(void) { return g_regular_size; }
uint64_t nexus_font_bold_size(void) { return g_bold_size; }
