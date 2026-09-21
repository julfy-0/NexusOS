#include "uefi_font.h"
#include "font8x16.h"

#include <stdint.h>
#include <stddef.h>

#if defined(__has_include)
#  if __has_include("stb_truetype.h")
#    define NEXUS_UEFI_HAS_STBTT 1
#  else
#    define NEXUS_UEFI_HAS_STBTT 0
#  endif
#else
#  define NEXUS_UEFI_HAS_STBTT 0
#endif


#if NEXUS_UEFI_HAS_STBTT

static UINTN nexus_uefi_strlen(const char *s) {
    UINTN n = 0;
    if (!s) return 0;
    while (s[n]) ++n;
    return n;
}


static EFI_BOOT_SERVICES *g_font_bs;

static int uefi_ifloor(double x) {
    int i = (int)x;
    if ((double)i > x) --i;
    return i;
}

static int uefi_iceil(double x) {
    int i = (int)x;
    if ((double)i < x) ++i;
    return i;
}

static double uefi_fabs(double x) {
    return x < 0.0 ? -x : x;
}

static double uefi_sqrt(double x) {
    if (x <= 0.0) return 0.0;
    double g = x > 1.0 ? x : 1.0;
    for (int i = 0; i < 16; ++i) g = 0.5 * (g + x / g);
    return g;
}

static double uefi_cuberoot(double x) {
    if (x == 0.0) return 0.0;
    int neg = x < 0.0;
    if (neg) x = -x;
    double g = x > 1.0 ? x : 1.0;
    for (int i = 0; i < 20; ++i) g = (2.0 * g + x / (g * g)) / 3.0;
    return neg ? -g : g;
}

static double uefi_pow(double x, double y) {
    if (y == 0.0) return 1.0;
    if (y > 0.333332 && y < 0.333335) return uefi_cuberoot(x);
    if (y < 0.0) return 1.0 / uefi_pow(x, -y);
    int whole = (int)y;
    if ((double)whole == y && whole >= 0 && whole <= 32) {
        double r = 1.0;
        for (int i = 0; i < whole; ++i) r *= x;
        return r;
    }
    return 0.0;
}

static double uefi_fmod(double x, double y) {
    if (y == 0.0) return 0.0;
    long long q = (long long)(x / y);
    return x - (double)q * y;
}

static double uefi_cos(double x) {
    const double pi = 3.14159265358979323846;
    const double two_pi = 6.28318530717958647692;
    x = uefi_fmod(x, two_pi);
    if (x > pi) x -= two_pi;
    if (x < -pi) x += two_pi;
    double x2 = x * x;
    return 1.0 - x2 * 0.5 + x2 * x2 / 24.0 - x2 * x2 * x2 / 720.0;
}

static double uefi_acos(double x) {
    const double pi = 3.14159265358979323846;
    if (x <= -1.0) return pi;
    if (x >= 1.0) return 0.0;
    double ax = uefi_fabs(x);
    double r = (-0.0187293 * ax + 0.0742610) * ax - 0.2121144;
    r = (r * ax + 1.5707288) * uefi_sqrt(1.0 - ax);
    return x < 0.0 ? pi - r : r;
}

static void *uefi_stb_malloc(size_t size, void *userdata) {
    (void)userdata;
    if (!g_font_bs || size == 0) return NULL;
    void *buffer = NULL;
    if (EFI_ERROR(g_font_bs->AllocatePool(EfiLoaderData, (UINTN)size, &buffer))) return NULL;
    return buffer;
}

static void uefi_stb_free(void *ptr, void *userdata) {
    (void)userdata;
    if (g_font_bs && ptr) (void)g_font_bs->FreePool(ptr);
}

static void *uefi_stb_memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

static void *uefi_stb_memset(void *dst, int value, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    while (n--) *d++ = (unsigned char)value;
    return dst;
}

static size_t uefi_stb_strlen(const char *s) {
    return (size_t)nexus_uefi_strlen(s);
}

static void uefi_stb_assert(int expression) {
    (void)expression;
}

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_ifloor(x)    uefi_ifloor(x)
#define STBTT_iceil(x)     uefi_iceil(x)
#define STBTT_sqrt(x)      uefi_sqrt(x)
#define STBTT_pow(x,y)     uefi_pow((x),(y))
#define STBTT_fmod(x,y)    uefi_fmod((x),(y))
#define STBTT_cos(x)       uefi_cos(x)
#define STBTT_acos(x)      uefi_acos(x)
#define STBTT_fabs(x)      uefi_fabs(x)
#define STBTT_malloc(x,u)  uefi_stb_malloc((x),(u))
#define STBTT_free(x,u)    uefi_stb_free((x),(u))
#define STBTT_memcpy       uefi_stb_memcpy
#define STBTT_memset       uefi_stb_memset
#define STBTT_strlen(x)    uefi_stb_strlen(x)
#define STBTT_assert(x)    uefi_stb_assert(x)
#include "stb_truetype.h"

static int load_ttf_file(EFI_BOOT_SERVICES *bs,
                         EFI_FILE_PROTOCOL *root,
                         CHAR16 *path,
                         unsigned char **out_data,
                         UINTN *out_size) {
    if (!bs || !root || !path || !out_data || !out_size) return 0;

    EFI_FILE_PROTOCOL *file = NULL;
    EFI_STATUS status = root->Open(root, &file, path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status) || !file) return 0;

    EFI_GUID info_guid = EFI_FILE_INFO_GUID;
    UINTN info_size = sizeof(EFI_FILE_INFO) + 512;
    EFI_FILE_INFO *info = NULL;
    status = bs->AllocatePool(EfiLoaderData, info_size, (void **)&info);
    if (EFI_ERROR(status) || !info) {
        file->Close(file);
        return 0;
    }

    status = file->GetInfo(file, &info_guid, &info_size, info);
    if (EFI_ERROR(status)) {
        bs->FreePool(info);
        file->Close(file);
        return 0;
    }

    UINTN file_size = (UINTN)info->FileSize;
    bs->FreePool(info);

    if (file_size == 0) {
        file->Close(file);
        return 0;
    }

    unsigned char *buffer = NULL;
    status = bs->AllocatePool(EfiLoaderData, file_size, (void **)&buffer);
    if (EFI_ERROR(status) || !buffer) {
        file->Close(file);
        return 0;
    }

    UINTN read_size = file_size;
    status = file->Read(file, &read_size, buffer);
    file->Close(file);
    if (EFI_ERROR(status) || read_size != file_size) {
        bs->FreePool(buffer);
        return 0;
    }

    *out_data = buffer;
    *out_size = file_size;
    return 1;
}

static uint32_t fb_pack(const EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
                        uint8_t r, uint8_t g, uint8_t b) {
    if (gop->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor)
        return ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void fb_unpack(const EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
                      uint32_t packed, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (gop->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
        *r = (uint8_t)(packed & 0xFF);
        *g = (uint8_t)((packed >> 8) & 0xFF);
        *b = (uint8_t)((packed >> 16) & 0xFF);
    } else {
        *b = (uint8_t)(packed & 0xFF);
        *g = (uint8_t)((packed >> 8) & 0xFF);
        *r = (uint8_t)((packed >> 16) & 0xFF);
    }
}

static void fb_blend(const EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
                     int x, int y,
                     uint8_t r, uint8_t g, uint8_t b,
                     uint8_t alpha) {
    if (x < 0 || y < 0) return;
    UINTN width = gop->Mode->Info->HorizontalResolution;
    UINTN height = gop->Mode->Info->VerticalResolution;
    if ((UINTN)x >= width || (UINTN)y >= height) return;
    if (gop->Mode->Info->PixelFormat != PixelRedGreenBlueReserved8BitPerColor &&
        gop->Mode->Info->PixelFormat != PixelBlueGreenRedReserved8BitPerColor) return;

    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)gop->Mode->FrameBufferBase;
    UINTN stride = gop->Mode->Info->PixelsPerScanLine;
    uint32_t old = fb[(UINTN)y * stride + (UINTN)x];
    if (alpha == 255) {
        fb[(UINTN)y * stride + (UINTN)x] = fb_pack(gop, r, g, b);
        return;
    }
    if (alpha == 0) return;

    uint8_t br, bg, bb;
    fb_unpack(gop, old, &br, &bg, &bb);
    uint8_t rr = (uint8_t)(((uint32_t)r * alpha + (uint32_t)br * (255U - alpha)) / 255U);
    uint8_t rg = (uint8_t)(((uint32_t)g * alpha + (uint32_t)bg * (255U - alpha)) / 255U);
    uint8_t rb = (uint8_t)(((uint32_t)b * alpha + (uint32_t)bb * (255U - alpha)) / 255U);
    fb[(UINTN)y * stride + (UINTN)x] = fb_pack(gop, rr, rg, rb);
}

#endif /* NEXUS_UEFI_HAS_STBTT */

#if !NEXUS_UEFI_HAS_STBTT

static int fallback_glyph_supported(unsigned char ch) {
    return ch >= 32 && ch < 128;
}

static int fallback_text_width(const char *text) {
    int width = 0;
    if (!text) return 0;
    for (UINTN i = 0; text[i]; ++i) {
        if (text[i] == '\n') break;
        if (fallback_glyph_supported((unsigned char)text[i])) width += 8;
    }
    return width;
}

static void fallback_draw_glyph(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
                                int x, int y,
                                unsigned char ch,
                                uint8_t r, uint8_t g, uint8_t b) {
    if (!fallback_glyph_supported(ch)) return;
    for (int yy = 0; yy < 16; ++yy) {
        uint8_t row = font8x16[ch][yy];
        for (int xx = 0; xx < 8; ++xx) {
            if (row & (1U << (7 - xx))) {
                if (x + xx < 0 || y + yy < 0) continue;
                if ((UINTN)(x + xx) >= gop->Mode->Info->HorizontalResolution ||
                    (UINTN)(y + yy) >= gop->Mode->Info->VerticalResolution) continue;
                volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)gop->Mode->FrameBufferBase;
                UINTN stride = gop->Mode->Info->PixelsPerScanLine;
                uint32_t packed = 0;
                if (gop->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor)
                    packed = ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
                else if (gop->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor)
                    packed = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
                else
                    continue;
                fb[(UINTN)(y + yy) * stride + (UINTN)(x + xx)] = packed;
            }
        }
    }
}

#endif

EFI_STATUS nexus_uefi_font_init(nexus_uefi_font_t *font,
                                EFI_BOOT_SERVICES *bs,
                                EFI_FILE_PROTOCOL *root,
                                CHAR16 *path,
                                int pixel_height) {
    if (!font || !bs || !root || !path || pixel_height <= 0) return EFI_LOAD_ERROR;
    for (UINTN i = 0; i < sizeof(*font); ++i) ((unsigned char *)font)[i] = 0;
    font->bs = bs;
    font->size_px = pixel_height;

#if NEXUS_UEFI_HAS_STBTT
    unsigned char *ttf = NULL;
    UINTN ttf_size = 0;
    if (!load_ttf_file(bs, root, path, &ttf, &ttf_size)) return EFI_NOT_FOUND;

    stbtt_fontinfo *info = NULL;
    EFI_STATUS status = bs->AllocatePool(EfiLoaderData, sizeof(*info), (void **)&info);
    if (EFI_ERROR(status) || !info) {
        bs->FreePool(ttf);
        return status;
    }

    g_font_bs = bs;
    int offset = stbtt_GetFontOffsetForIndex(ttf, 0);
    if (offset < 0 || !stbtt_InitFont(info, ttf, offset)) {
        bs->FreePool(info);
        bs->FreePool(ttf);
        g_font_bs = NULL;
        return EFI_LOAD_ERROR;
    }

    float scale = stbtt_ScaleForPixelHeight(info, (float)pixel_height);
    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(info, &ascent, &descent, &line_gap);
    font->ascent_px = (int)(ascent * scale);
    font->descent_px = (int)(descent * scale);
    font->line_gap_px = (int)(line_gap * scale);
    font->ttf_data = ttf;
    font->ttf_size = ttf_size;
    font->font_state = info;
    font->using_stb = 1;
    font->ready = 1;
    return EFI_SUCCESS;
#else
    (void)bs;
    (void)root;
    (void)path;
    font->using_stb = 0;
    font->ready = 1;
    font->ascent_px = 12;
    font->descent_px = 4;
    font->line_gap_px = 0;
    return EFI_SUCCESS;
#endif
}

void nexus_uefi_font_shutdown(nexus_uefi_font_t *font) {
    if (!font) return;
#if NEXUS_UEFI_HAS_STBTT
    if (font->bs && font->font_state) (void)font->bs->FreePool(font->font_state);
    if (font->bs && font->ttf_data) (void)font->bs->FreePool(font->ttf_data);
    if (g_font_bs == font->bs) g_font_bs = NULL;
#endif
    font->font_state = NULL;
    font->ttf_data = NULL;
    font->ttf_size = 0;
    font->ready = 0;
}

void nexus_uefi_font_draw_text(nexus_uefi_font_t *font,
                               EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
                               const char *text,
                               int x,
                               int baseline_y,
                               uint8_t r,
                               uint8_t g,
                               uint8_t b) {
    if (!font || !font->ready || !gop || !gop->Mode || !gop->Mode->Info || !text) return;

#if NEXUS_UEFI_HAS_STBTT
    if (font->using_stb && font->font_state) {
        stbtt_fontinfo *info = (stbtt_fontinfo *)font->font_state;
        float scale = stbtt_ScaleForPixelHeight(info, (float)font->size_px);
        float xpos = (float)x;
        UINTN len = nexus_uefi_strlen(text);
        for (UINTN i = 0; i < len; ++i) {
            unsigned char ch = (unsigned char)text[i];
            if (ch == '\n') {
                xpos = (float)x;
                baseline_y += font->size_px + font->line_gap_px;
                continue;
            }
            int advance = 0, lsb = 0;
            stbtt_GetCodepointHMetrics(info, ch, &advance, &lsb);
            float x_shift = xpos - (float)(int)xpos;
            int w = 0, h = 0, xoff = 0, yoff = 0;
            unsigned char *bitmap = stbtt_GetCodepointBitmap(info, scale, scale, ch,
                                                             &w, &h, &xoff, &yoff);
            if (bitmap) {
                int draw_x = (int)xpos + xoff;
                int draw_y = baseline_y + yoff;
                for (int yy = 0; yy < h; ++yy) {
                    for (int xx = 0; xx < w; ++xx) {
                        uint8_t alpha = bitmap[yy * w + xx];
                        if (alpha) fb_blend(gop, draw_x + xx, draw_y + yy, r, g, b, alpha);
                    }
                }
                stbtt_FreeBitmap(bitmap, info->userdata);
            }
            xpos += (float)advance * scale;
            if (i + 1 < len) {
                unsigned char next = (unsigned char)text[i + 1];
                xpos += stbtt_GetCodepointKernAdvance(info, ch, next) * scale;
            }
            (void)x_shift;
            (void)lsb;
        }
        return;
    }
#endif

#if !NEXUS_UEFI_HAS_STBTT
    int cursor_x = x;
    int cursor_y = baseline_y - 12;
    for (UINTN i = 0; text[i]; ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (ch == '\n') {
            cursor_x = x;
            cursor_y += 16;
            continue;
        }
        fallback_draw_glyph(gop, cursor_x, cursor_y, ch, r, g, b);
        cursor_x += 8;
    }
#else
    (void)x; (void)baseline_y; (void)r; (void)g; (void)b;
#endif
}

void nexus_uefi_font_draw_text_centered(nexus_uefi_font_t *font,
                                        EFI_GRAPHICS_OUTPUT_PROTOCOL *gop,
                                        const char *text,
                                        int screen_width,
                                        int baseline_y,
                                        uint8_t r,
                                        uint8_t g,
                                        uint8_t b) {
    if (!font || !font->ready || !text) return;
#if NEXUS_UEFI_HAS_STBTT
    if (font->using_stb && font->font_state) {
        stbtt_fontinfo *info = (stbtt_fontinfo *)font->font_state;
        float scale = stbtt_ScaleForPixelHeight(info, (float)font->size_px);
        float width = 0.0f;
        UINTN len = nexus_uefi_strlen(text);
        for (UINTN i = 0; i < len; ++i) {
            unsigned char ch = (unsigned char)text[i];
            if (ch == '\n') break;
            int advance = 0, lsb = 0;
            stbtt_GetCodepointHMetrics(info, ch, &advance, &lsb);
            width += (float)advance * scale;
            if (i + 1 < len) width += stbtt_GetCodepointKernAdvance(info, ch, (unsigned char)text[i + 1]) * scale;
        }
        nexus_uefi_font_draw_text(font, gop, text, (int)((float)screen_width * 0.5f - width * 0.5f), baseline_y, r, g, b);
        return;
    }
#endif
    nexus_uefi_font_draw_text(font, gop, text, (screen_width - fallback_text_width(text)) / 2,
                               baseline_y, r, g, b);
}
