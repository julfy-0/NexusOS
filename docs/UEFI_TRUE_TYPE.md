# NexusOS 0.7.0 — UEFI TrueType Boot Text

The UEFI boot path now has a dedicated font-rendering layer in `boot/uefi/src/uefi_font.c`.

## Runtime path

1. UEFI GOP is initialized and the active framebuffer is retained.
2. The boot volume is opened through `EFI_SIMPLE_FILE_SYSTEM_PROTOCOL`.
3. `\\EFI\\BOOT\\FONT.TTF` is staged from `system/services/font/Roboto-Regular.ttf`.
4. `nexus_uefi_font_init()` reads the TTF into a UEFI pool allocation.
5. When `stb_truetype.h` is present, the renderer initializes `stbtt_fontinfo`, computes the pixel scale, renders glyph bitmaps, and alpha-blends them into GOP video memory.
6. The font buffer is released before the final `GetMemoryMap()` / `ExitBootServices()` sequence.
7. When the header is not present, the existing 8x16 bitmap font is used as a build-safe fallback.

## UEFI integration boundary

The renderer replaces libc dependencies used by the stb implementation with UEFI-safe callbacks:

- allocation/free → `EFI_BOOT_SERVICES->AllocatePool()` / `FreePool()`;
- string length → local implementation;
- memcpy/memset → local freestanding loops;
- floor/ceil/fabs → local arithmetic implementations;
- sqrt → Newton iteration;
- fmod/cos/acos/pow → freestanding approximations sufficient for the stb rasterizer path used by text rendering;
- assertions → no-op.

## Framebuffer blending

Glyph bitmaps are treated as 8-bit coverage masks. The renderer reads the existing GOP pixel and performs:

`result = (text * alpha + background * (255 - alpha)) / 255`

for each RGB component.

## External dependency status

The integration layer targets `stb_truetype.h` v1.26 from `nothings/stb`. The current development container did not have a local copy of the single-header library, so the archive contains the integration layer plus the fallback renderer, but does not claim that the external header itself was re-vendored here.

Official source: https://github.com/nothings/stb/blob/master/stb_truetype.h
