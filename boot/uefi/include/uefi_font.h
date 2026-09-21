#ifndef NEXUSOS_UEFI_FONT_H
#define NEXUSOS_UEFI_FONT_H

#include "efi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int ready;
    int using_stb;
    int size_px;
    int ascent_px;
    int descent_px;
    int line_gap_px;
    unsigned char *ttf_data;
    UINTN ttf_size;
    EFI_BOOT_SERVICES *bs;
    void *font_state;
} nexus_uefi_font_t;

EFI_STATUS nexus_uefi_font_init(nexus_uefi_font_t *font,
                                EFI_BOOT_SERVICES *bs,
                                EFI_FILE_PROTOCOL *root,
                                CHAR16 *path,
                                int pixel_height);

void nexus_uefi_font_shutdown(nexus_uefi_font_t *font);

void nexus_uefi_font_draw_text(nexus_uefi_font_t *font,
                               EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
                               const char *text,
                               int x,
                               int baseline_y,
                               uint8_t r,
                               uint8_t g,
                               uint8_t b);

void nexus_uefi_font_draw_text_centered(nexus_uefi_font_t *font,
                                        EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
                                        const char *text,
                                        int screen_width,
                                        int baseline_y,
                                        uint8_t r,
                                        uint8_t g,
                                        uint8_t b);

#ifdef __cplusplus
}
#endif

#endif
