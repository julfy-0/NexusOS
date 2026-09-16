/*
 * NexusOS Bootloader (BOOTX64.EFI)
 *
 * 1. печатает баннер и находит видеорежим (GOP)
 * 2. открывает \kernel.elf на том же диске, с которого сам загрузился
 * 3. парсит ELF64, раскладывает PT_LOAD-сегменты по нужным физическим адресам
 * 4. получает финальную memory map, зовёт ExitBootServices()
 * 5. прыгает в точку входа ядра, передавая nexus_boot_info_t*
 */

#include "efi.h"
#include "elf.h"
#include "boot_info.h"
#include "nexus_version.h"
#include "nexus_logo.h"

extern void *memcpy(void *dest, const void *src, unsigned long n);
extern void *memset(void *dest, int value, unsigned long n);

static EFI_SYSTEM_TABLE *g_st;
static EFI_BOOT_SERVICES *g_bs;

static void print(CHAR16 *str) {
    g_st->ConOut->OutputString(g_st->ConOut, str);
}



/* Modern graphical NexusOS boot screen.
 * Uses UEFI GOP directly, so every element is positioned in pixels and
 * automatically stays centered at any screen resolution. */
static EFI_GRAPHICS_OUTPUT_PROTOCOL *g_gop = NULL;
static UINTN g_width = 0, g_height = 0, g_stride = 0;
static UINTN g_progress_y = 0;

static uint32_t pixel_value(uint8_t r, uint8_t g, uint8_t b) {
    if (g_gop->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor)
        return ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void put_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || (UINTN)x >= g_width || (UINTN)y >= g_height) return;
    volatile uint32_t *fb = (volatile uint32_t *)(uintptr_t)g_gop->Mode->FrameBufferBase;
    fb[(UINTN)y * g_stride + (UINTN)x] = pixel_value(r, g, b);
}

static void fill_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    for (int yy = 0; yy < h; ++yy)
        for (int xx = 0; xx < w; ++xx)
            put_pixel(x + xx, y + yy, r, g, b);
}

static void clear_black(void) {
    fill_rect(0, 0, (int)g_width, (int)g_height, 0, 0, 0);
}

/* Draw the supplied NexusOS logo from its embedded RGB565 RLE asset. */
static void draw_logo(void) {
    int x = ((int)g_width - NEXUS_LOGO_WIDTH) / 2;
    int y = ((int)g_height - NEXUS_LOGO_HEIGHT) / 2 - 20;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    int px_index = 0;
    for (UINTN i = 0; i < NEXUS_LOGO_RUNS; ++i) {
        UINTN count = nexus_logo_rle[i][0];
        uint16_t color = nexus_logo_rle[i][1];
        for (UINTN j = 0; j < count; ++j) {
            int local_x = px_index % NEXUS_LOGO_WIDTH;
            int local_y = px_index / NEXUS_LOGO_WIDTH;
            if (local_y >= NEXUS_LOGO_HEIGHT) break;
            uint16_t p = color;
            uint8_t r = (uint8_t)(((p >> 11) & 0x1F) * 255 / 31);
            uint8_t g = (uint8_t)(((p >> 5) & 0x3F) * 255 / 63);
            uint8_t b = (uint8_t)((p & 0x1F) * 255 / 31);
            put_pixel(x + local_x, y + local_y, r, g, b);
            ++px_index;
        }
    }
    g_progress_y = y + NEXUS_LOGO_HEIGHT + (int)(g_height / 14);
}

static void boot_graphics_init(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop) {
    g_gop = gop;
    g_width = gop->Mode->Info->HorizontalResolution;
    g_height = gop->Mode->Info->VerticalResolution;
    g_stride = gop->Mode->Info->PixelsPerScanLine;
    clear_black();
    draw_logo();
}

static void boot_progress(UINTN current, UINTN total, CHAR16 *label) {
    (void)label;
    if (!g_gop || !total) return;
    int bar_w = (int)(g_width * 38 / 100);
    if (bar_w < 220) bar_w = 220;
    if ((UINTN)bar_w > g_width - 40) bar_w = (int)g_width - 40;
    int bar_h = (int)(g_height / 90); if (bar_h < 8) bar_h = 8; if (bar_h > 20) bar_h = 20;
    int x = ((int)g_width - bar_w) / 2;
    int y = (int)g_progress_y;
    int filled = (int)((current * (UINTN)bar_w) / total);
    fill_rect(x, y, bar_w, bar_h, 42, 42, 42);
    for (int xx=0; xx<filled; ++xx) {
        uint8_t v = (uint8_t)(255 - (xx * 120 / (bar_w ? bar_w : 1)));
        fill_rect(x+xx, y, 1, bar_h, v, v, v);
    }
    if (g_bs) g_bs->Stall(80000);
}

static void panic(CHAR16 *msg) {
    g_st->ConOut->SetAttribute(g_st->ConOut, EFI_RED | EFI_BACKGROUND_BLACK);
    print(u"PANIC: ");
    print(msg);
    print(u"\r\n");
    for (;;) {
        g_bs->Stall(1000000);
    }
}

/* Читает весь файл целиком в буфер, выделенный через AllocatePool.
 * Возвращает адрес буфера и записывает размер в *out_size. */
static void *load_file(EFI_FILE_PROTOCOL *root, CHAR16 *name, UINTN *out_size) {
    EFI_FILE_PROTOCOL *file;
    EFI_STATUS status = root->Open(root, &file, name, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(status)) {
        panic(u"cannot open file");
    }

    /* Узнаём размер файла через GetInfo(EFI_FILE_INFO_GUID) */
    EFI_GUID info_guid = EFI_FILE_INFO_GUID;
    UINTN info_size = sizeof(EFI_FILE_INFO) + 512; /* с запасом под имя файла */
    EFI_FILE_INFO *info;
    g_bs->AllocatePool(EfiLoaderData, info_size, (void **)&info);
    status = file->GetInfo(file, &info_guid, &info_size, info);
    if (EFI_ERROR(status)) {
        panic(u"GetInfo failed");
    }

    UINTN file_size = info->FileSize;
    g_bs->FreePool(info);

    void *buffer;
    status = g_bs->AllocatePool(EfiLoaderData, file_size, &buffer);
    if (EFI_ERROR(status)) {
        panic(u"AllocatePool failed");
    }

    UINTN read_size = file_size;
    status = file->Read(file, &read_size, buffer);
    if (EFI_ERROR(status)) {
        panic(u"Read failed");
    }

    file->Close(file);
    *out_size = file_size;
    return buffer;
}

/* -------------------------------------------------------------------------
 * ELF loader
 * -------------------------------------------------------------------------
 *
 * The kernel is linked at NEXUS_KERNEL_LINK_BASE but is emitted as ET_DYN.
 * We therefore allocate one contiguous physical image below 4 GiB, copy all
 * PT_LOAD segments into it, and apply only R_X86_64_RELATIVE relocations.
 * This removes the old dependency on AllocateAddress(0x200000) and makes
 * the boot path independent of whatever low memory UEFI/OVMF happens to use.
 */
typedef struct {
    uint64_t phys_base;
    uint64_t phys_end;
    uint64_t link_base;
    uint64_t entry;
    uint64_t image_size;
} kernel_load_result_t;

static uint64_t align_down_4k(uint64_t value) {
    return value & ~0xFFFULL;
}

static uint64_t align_up_4k_checked(uint64_t value) {
    if (value > UINT64_MAX - 0xFFFULL) return 0;
    return (value + 0xFFFULL) & ~0xFFFULL;
}

static int range_valid(uint64_t start, uint64_t size, uint64_t *end_out) {
    if (size > UINT64_MAX - start) return 0;
    if (end_out) *end_out = start + size;
    return 1;
}

static int is_power_of_two(uint64_t value) {
    return value == 0 || (value & (value - 1ULL)) == 0;
}

static void *allocate_low_pages(UINTN pages, EFI_PHYSICAL_ADDRESS *out_address) {
    if (!pages || !out_address) return NULL;

    EFI_PHYSICAL_ADDRESS max_address = 0xFFFFFFFFULL;
    EFI_STATUS status = g_bs->AllocatePages(AllocateMaxAddress, EfiLoaderData,
                                             pages, &max_address);
    if (EFI_ERROR(status)) return NULL;
    *out_address = max_address;
    return (void *)(uintptr_t)max_address;
}

static int apply_relative_relocations(uint64_t load_base,
                                      uint64_t link_base,
                                      uint64_t image_size,
                                      uint64_t dyn_vaddr,
                                      uint64_t dyn_size,
                                      uint64_t min_vaddr) {
    if (dyn_size < sizeof(Elf64_Dyn)) return 0;
    if (dyn_vaddr < min_vaddr || dyn_vaddr - min_vaddr >= image_size) return 0;
    if (dyn_size > image_size - (dyn_vaddr - min_vaddr)) return 0;

    Elf64_Dyn *dyn = (Elf64_Dyn *)(uintptr_t)(load_base + (dyn_vaddr - min_vaddr));
    uint64_t rela_vaddr = 0;
    uint64_t rela_size = 0;
    uint64_t rela_ent = sizeof(Elf64_Rela);

    uint64_t dyn_count = dyn_size / sizeof(Elf64_Dyn);
    for (uint64_t i = 0; i < dyn_count; ++i) {
        switch ((uint64_t)dyn[i].d_tag) {
            case DT_RELA:    rela_vaddr = dyn[i].d_val; break;
            case DT_RELASZ:  rela_size = dyn[i].d_val; break;
            case DT_RELAENT: rela_ent = dyn[i].d_val; break;
            case DT_NULL:    i = dyn_count; break;
            default: break;
        }
    }

    if (rela_size == 0) return 1;
    if (rela_ent != sizeof(Elf64_Rela) || (rela_size % rela_ent) != 0) return 0;
    if (rela_vaddr < min_vaddr || rela_vaddr - min_vaddr >= image_size) return 0;
    if (rela_size > image_size - (rela_vaddr - min_vaddr)) return 0;

    Elf64_Rela *rela = (Elf64_Rela *)(uintptr_t)(load_base + (rela_vaddr - min_vaddr));
    uint64_t count = rela_size / rela_ent;
    uint64_t load_bias = load_base - link_base;

    for (uint64_t i = 0; i < count; ++i) {
        uint32_t type = ELF64_R_TYPE(rela[i].r_info);
        if (type != R_X86_64_RELATIVE) {
            panic(u"kernel.elf: unsupported relocation");
        }

        if (rela[i].r_offset < min_vaddr ||
            rela[i].r_offset - min_vaddr > image_size - sizeof(uint64_t)) {
            return 0;
        }

        uint64_t *where = (uint64_t *)(uintptr_t)(load_base + (rela[i].r_offset - min_vaddr));
        uint64_t value = load_bias + (uint64_t)rela[i].r_addend;
        *where = value;
    }

    return 1;
}

static int load_elf(void *elf_data, UINTN file_size, kernel_load_result_t *result) {
    if (!elf_data || !result || file_size < sizeof(Elf64_Ehdr)) {
        panic(u"kernel.elf: truncated header");
    }

    Elf64_Ehdr *eh = (Elf64_Ehdr *)elf_data;
    if (eh->e_ident[0] != ELF_MAGIC0 || eh->e_ident[1] != ELF_MAGIC1 ||
        eh->e_ident[2] != ELF_MAGIC2 || eh->e_ident[3] != ELF_MAGIC3) {
        panic(u"kernel.elf: bad magic");
    }
    if (eh->e_ident[4] != ELFCLASS64 || eh->e_ident[5] != ELFDATA2LSB) {
        panic(u"kernel.elf: unsupported format");
    }
    if (eh->e_machine != EM_X86_64 || eh->e_type != ET_DYN) {
        panic(u"kernel.elf: expected relocatable ET_DYN x86_64 image");
    }
    if (eh->e_phentsize < sizeof(Elf64_Phdr) || eh->e_phnum == 0) {
        panic(u"kernel.elf: invalid program headers");
    }
    if (eh->e_phoff > file_size ||
        (uint64_t)eh->e_phnum > ((uint64_t)file_size - eh->e_phoff) / eh->e_phentsize) {
        panic(u"kernel.elf: program headers outside file");
    }

    Elf64_Phdr *phdrs = (Elf64_Phdr *)((uint8_t *)elf_data + eh->e_phoff);
    uint64_t min_vaddr = UINT64_MAX;
    uint64_t max_vaddr = 0;
    uint64_t dyn_vaddr = 0;
    uint64_t dyn_size = 0;
    int load_count = 0;

    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        Elf64_Phdr *ph = &phdrs[i];
        if (ph->p_type == PT_DYNAMIC) {
            dyn_vaddr = ph->p_vaddr;
            dyn_size = ph->p_memsz;
            continue;
        }
        if (ph->p_type != PT_LOAD) continue;
        ++load_count;

        uint64_t file_end, mem_end;
        if (!range_valid(ph->p_offset, ph->p_filesz, &file_end) ||
            file_end > file_size || ph->p_memsz < ph->p_filesz ||
            !range_valid(ph->p_vaddr, ph->p_memsz, &mem_end) ||
            !is_power_of_two(ph->p_align)) {
            panic(u"kernel.elf: invalid load segment");
        }

        uint64_t seg_start = align_down_4k(ph->p_vaddr);
        uint64_t seg_end = align_up_4k_checked(mem_end);
        if (seg_end == 0 || seg_end <= seg_start) {
            panic(u"kernel.elf: invalid segment range");
        }
        if (seg_start < min_vaddr) min_vaddr = seg_start;
        if (seg_end > max_vaddr) max_vaddr = seg_end;
    }

    if (load_count == 0 || min_vaddr == UINT64_MAX || max_vaddr <= min_vaddr) {
        panic(u"kernel.elf: no loadable segments");
    }

    /* The current kernel MMU deliberately requires the image to remain below
     * 4 GiB. AllocateMaxAddress gives us the strongest possible UEFI guarantee. */
    uint64_t image_size = max_vaddr - min_vaddr;
    if (min_vaddr != NEXUS_KERNEL_LINK_BASE || image_size >= 0x100000000ULL) {
        panic(u"kernel.elf: unsupported image layout");
    }
    UINTN pages = (UINTN)(image_size / 4096ULL);
    if (pages == 0 || (uint64_t)pages * 4096ULL != image_size) {
        panic(u"kernel.elf: image size overflow");
    }

    EFI_PHYSICAL_ADDRESS actual_base = 0;
    if (!allocate_low_pages(pages, &actual_base)) {
        panic(u"kernel.elf: cannot allocate physical image below 4 GiB");
    }

    memset((void *)(uintptr_t)actual_base, 0, image_size);

    for (uint16_t i = 0; i < eh->e_phnum; ++i) {
        Elf64_Phdr *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD || ph->p_filesz == 0) continue;

        uint64_t dest_offset = ph->p_vaddr - min_vaddr;
        if (dest_offset > image_size || ph->p_filesz > image_size - dest_offset) {
            panic(u"kernel.elf: segment outside allocated image");
        }
        memcpy((void *)(uintptr_t)(actual_base + dest_offset),
               (uint8_t *)elf_data + ph->p_offset,
               ph->p_filesz);
    }

    if (dyn_vaddr == 0 || dyn_size == 0 ||
        !apply_relative_relocations(actual_base, min_vaddr, image_size,
                                    dyn_vaddr, dyn_size, min_vaddr)) {
        panic(u"kernel.elf: relocation processing failed");
    }

    if (eh->e_entry < min_vaddr || eh->e_entry >= max_vaddr) {
        panic(u"kernel.elf: entry outside image");
    }

    result->phys_base = actual_base;
    result->phys_end = actual_base + image_size;
    result->link_base = min_vaddr;
    result->entry = actual_base + (eh->e_entry - min_vaddr);
    result->image_size = image_size;
    return 1;
}

static int guid_equal(const EFI_GUID *a, const EFI_GUID *b) {
    return a->Data1 == b->Data1 && a->Data2 == b->Data2 &&
           a->Data3 == b->Data3 &&
           a->Data4[0] == b->Data4[0] && a->Data4[1] == b->Data4[1] &&
           a->Data4[2] == b->Data4[2] && a->Data4[3] == b->Data4[3] &&
           a->Data4[4] == b->Data4[4] && a->Data4[5] == b->Data4[5] &&
           a->Data4[6] == b->Data4[6] && a->Data4[7] == b->Data4[7];
}

static int ascii_copy_smbios_string(uint8_t *table, uint8_t *end,
                                    uint8_t wanted_index, char *out, UINTN out_size) {
    if (!wanted_index || !out || out_size == 0) return 0;
    uint8_t *p = table;
    uint8_t index = 1;
    while (p < end && *p) {
        if (index == wanted_index) {
            UINTN n = 0;
            while (p < end && *p && n + 1 < out_size) out[n++] = (char)*p++;
            out[n] = '\0';
            return n != 0;
        }
        while (p < end && *p) p++;
        if (p < end) p++;
        index++;
    }
    out[0] = '\0';
    return 0;
}

static void detect_acpi(nexus_boot_info_t *bi) {
    bi->acpi_valid = 0;
    bi->acpi_revision = 0;
    bi->acpi_rsdp = 0;
    if (!g_st || !g_st->ConfigurationTable || !g_st->NumberOfTableEntries) return;

    /* ACPI 2.0+ and ACPI 1.0 EFI configuration table GUIDs. */
    EFI_GUID acpi20 = {0x8868E871,0xE4F1,0x11D3,{0xBC,0x22,0x00,0x80,0xC7,0x3C,0x88,0x81}};
    EFI_GUID acpi10 = {0xEB9D2D30,0x2D88,0x11D3,{0x9A,0x16,0x00,0x90,0x27,0x3F,0xC1,0x4D}};
    EFI_CONFIGURATION_TABLE *tables = (EFI_CONFIGURATION_TABLE *)g_st->ConfigurationTable;

    /* Prefer ACPI 2.0+ when both entries are present. */
    for (int pass = 0; pass < 2; ++pass) {
        for (UINTN i = 0; i < g_st->NumberOfTableEntries; ++i) {
            EFI_GUID *wanted = pass == 0 ? &acpi20 : &acpi10;
            if (!guid_equal(&tables[i].VendorGuid, wanted)) continue;
            uint8_t *rsdp = (uint8_t *)tables[i].VendorTable;
            if (!rsdp) continue;
            if (rsdp[0]=='R' && rsdp[1]=='S' && rsdp[2]=='D' && rsdp[3]==' ' &&
                rsdp[4]=='P' && rsdp[5]=='T' && rsdp[6]=='R' && rsdp[7]==' ') {
                bi->acpi_valid = 1;
                bi->acpi_revision = rsdp[15];
                bi->acpi_rsdp = (uint64_t)(uintptr_t)rsdp;
                return;
            }
        }
    }
}

static void detect_system_identity(nexus_boot_info_t *bi) {
    bi->system_info_valid = 0;
    bi->system_manufacturer[0] = '\0';
    bi->system_product[0] = '\0';
    bi->baseboard_manufacturer[0] = '\0';
    if (!g_st || !g_st->ConfigurationTable || !g_st->NumberOfTableEntries) return;

    /* SMBIOS 2.x and 3.x EFI configuration table GUIDs. */
    EFI_GUID smbios2 = {0xEB9D2D31,0x2D88,0x11D3,{0x9A,0x16,0x00,0x90,0x27,0x3F,0xC1,0x4D}};
    EFI_GUID smbios3 = {0xF2FD1544,0x9794,0x4A2C,{0x99,0x2E,0xE5,0xBB,0xCF,0x20,0xE3,0x94}};
    EFI_CONFIGURATION_TABLE *tables = (EFI_CONFIGURATION_TABLE *)g_st->ConfigurationTable;
    uint8_t *table = NULL;
    UINTN table_len = 0;

    for (UINTN i = 0; i < g_st->NumberOfTableEntries; i++) {
        if (guid_equal(&tables[i].VendorGuid, &smbios2) ||
            guid_equal(&tables[i].VendorGuid, &smbios3)) {
            uint8_t *ep = (uint8_t *)tables[i].VendorTable;
            if (!ep) continue;
            if (ep[0]=='_' && ep[1]=='S' && ep[2]=='M' && ep[3]=='_' && ep[5] >= 0x1F) {
                uint16_t len = *(uint16_t *)(ep + 22);
                uint32_t addr = *(uint32_t *)(ep + 24);
                table = (uint8_t *)(uintptr_t)addr;
                table_len = len;
                break;
            }
            if (ep[0]=='_' && ep[1]=='S' && ep[2]=='M' && ep[3]=='3' && ep[4]=='_') {
                uint32_t maxlen = *(uint32_t *)(ep + 16);
                uint64_t addr = *(uint64_t *)(ep + 20);
                table = (uint8_t *)(uintptr_t)addr;
                table_len = maxlen;
                break;
            }
        }
    }
    if (!table || table_len < 4) return;

    uint8_t *end = table + table_len;
    for (uint8_t *p = table; p + 4 <= end; ) {
        uint8_t type = p[0];
        uint8_t len = p[1];
        if (len < 4 || p + len > end) break;
        if (type == 1) { /* System Information */
            uint8_t manufacturer = p[4];
            uint8_t product = p[5];
            uint8_t *strings = p + len;
            int got_manufacturer = ascii_copy_smbios_string(strings, end, manufacturer,
                                          bi->system_manufacturer, sizeof(bi->system_manufacturer));
            int got_product = ascii_copy_smbios_string(strings, end, product,
                                          bi->system_product, sizeof(bi->system_product));
            if (got_manufacturer || got_product) bi->system_info_valid = 1;
        } else if (type == 2) { /* Baseboard Information */
            uint8_t manufacturer = p[4];
            uint8_t *strings = p + len;
            if (ascii_copy_smbios_string(strings, end, manufacturer,
                                         bi->baseboard_manufacturer, sizeof(bi->baseboard_manufacturer))) {
                /* Baseboard manufacturer is enough for the interactive shell host. */
            }
        }
        uint8_t *q = p + len;
        while (q + 1 < end && !(q[0] == 0 && q[1] == 0)) q++;
        if (q + 1 >= end) break;
        p = q + 2;
    }
}

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    g_st = SystemTable;
    g_bs = SystemTable->BootServices;

    /* All long-lived hand-off data is explicitly placed below 4 GiB because
     * the first NexusOS address space is identity-mapped only in that range. */
    EFI_PHYSICAL_ADDRESS boot_info_phys = 0;
    nexus_boot_info_t *boot_info = (nexus_boot_info_t *)allocate_low_pages(1, &boot_info_phys);
    if (!boot_info) panic(u"cannot allocate boot information page");
    memset(boot_info, 0, 4096);
    boot_info->magic = NEXUS_BOOT_MAGIC;

    detect_system_identity(boot_info);
    detect_acpi(boot_info);

    /* ---- GOP ---- */
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_STATUS status = g_bs->LocateProtocol(&gop_guid, NULL, (void **)&gop);
    if (EFI_ERROR(status) || gop == NULL || gop->Mode == NULL || gop->Mode->Info == NULL) {
        panic(u"GOP not found - cannot continue without a framebuffer");
    }

    boot_info->fb.base = gop->Mode->FrameBufferBase;
    boot_info->fb.size = gop->Mode->FrameBufferSize;
    boot_info->fb.width = gop->Mode->Info->HorizontalResolution;
    boot_info->fb.height = gop->Mode->Info->VerticalResolution;
    boot_info->fb.pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine;

    if (gop->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
        boot_info->fb.pixel_format = NEXUS_PIXFMT_RGB;
    } else if (gop->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
        boot_info->fb.pixel_format = NEXUS_PIXFMT_BGR;
    } else {
        boot_info->fb.pixel_format = NEXUS_PIXFMT_OTHER;
    }
    if (boot_info->fb.pixel_format == NEXUS_PIXFMT_OTHER) {
        panic(u"Unsupported framebuffer pixel format");
    }

    boot_graphics_init(gop);
    boot_progress(1, 6, u"");

    /* ---- Locate boot volume ---- */
    EFI_GUID loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;
    status = g_bs->HandleProtocol(ImageHandle, &loaded_image_guid, (void **)&loaded_image);
    if (EFI_ERROR(status) || !loaded_image) panic(u"cannot get LoadedImageProtocol");

    EFI_GUID sfs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs = NULL;
    status = g_bs->HandleProtocol(loaded_image->DeviceHandle, &sfs_guid, (void **)&sfs);
    if (EFI_ERROR(status) || !sfs) panic(u"cannot get SimpleFileSystemProtocol");

    EFI_FILE_PROTOCOL *root = NULL;
    status = sfs->OpenVolume(sfs, &root);
    if (EFI_ERROR(status) || !root) panic(u"OpenVolume failed");

    boot_progress(2, 6, u"");
    UINTN kernel_size = 0;
    void *kernel_data = load_file(root, u"\\kernel.elf", &kernel_size);

    boot_progress(3, 6, u"");
    kernel_load_result_t kernel;
    if (!load_elf(kernel_data, kernel_size, &kernel)) {
        panic(u"kernel.elf: load failed");
    }

    /* No references into the ELF file are needed after relocation. Releasing
     * this buffer before the final memory map keeps the hand-off minimal. */
    g_bs->FreePool(kernel_data);
    kernel_data = NULL;
    root->Close(root);

    boot_info->kernel_phys_base = kernel.phys_base;
    boot_info->kernel_phys_end = kernel.phys_end;
    boot_info->kernel_image_size = kernel.image_size;
    boot_info->kernel_link_base = kernel.link_base;
    boot_info->kernel_entry = kernel.entry;

    boot_progress(4, 6, u"");

    /* ---- Final memory map ---- */
    UINTN mmap_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    uint32_t desc_version = 0;
    status = g_bs->GetMemoryMap(&mmap_size, NULL, &map_key, &desc_size, &desc_version);
    if (desc_size == 0) panic(u"GetMemoryMap returned invalid descriptor size");

    mmap_size += desc_size * 16 + 4096;
    UINTN mmap_pages = (mmap_size + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS mmap_phys = 0;
    EFI_MEMORY_DESCRIPTOR *mmap = (EFI_MEMORY_DESCRIPTOR *)allocate_low_pages(mmap_pages, &mmap_phys);
    if (!mmap) panic(u"cannot allocate final memory map");

    for (;;) {
        UINTN current_size = mmap_pages * 4096;
        status = g_bs->GetMemoryMap(&current_size, mmap, &map_key, &desc_size, &desc_version);
        if (!EFI_ERROR(status)) {
            mmap_size = current_size;
            break;
        }
        if (status != EFI_BUFFER_TOO_SMALL) panic(u"GetMemoryMap (final) failed");
        if (g_bs->FreePages) g_bs->FreePages(mmap_phys, mmap_pages);
        mmap_pages = (current_size + desc_size * 16 + 4095) / 4096;
        if (mmap_pages == 0) panic(u"GetMemoryMap size overflow");
        mmap = (EFI_MEMORY_DESCRIPTOR *)allocate_low_pages(mmap_pages, &mmap_phys);
        if (!mmap) panic(u"cannot grow final memory map");
    }

    boot_info->mmap.map_base = (uint64_t)(uintptr_t)mmap;
    boot_info->mmap.map_size = mmap_size;
    boot_info->mmap.descriptor_size = desc_size;
    boot_info->mmap.descriptor_version = desc_version;

    boot_progress(5, 6, u"");

    /* Do not call any Boot Service between this final GetMemoryMap and
     * ExitBootServices. If the key is stale, refresh the map immediately. */
    status = g_bs->ExitBootServices(ImageHandle, map_key);
    if (EFI_ERROR(status)) {
        UINTN retry_size = mmap_pages * 4096;
        status = g_bs->GetMemoryMap(&retry_size, mmap, &map_key, &desc_size, &desc_version);
        if (EFI_ERROR(status)) panic(u"GetMemoryMap retry failed");
        boot_info->mmap.map_size = retry_size;
        boot_info->mmap.descriptor_size = desc_size;
        boot_info->mmap.descriptor_version = desc_version;
        status = g_bs->ExitBootServices(ImageHandle, map_key);
        if (EFI_ERROR(status)) panic(u"ExitBootServices failed");
    }

    typedef void (*kernel_entry_t)(nexus_boot_info_t *);
    kernel_entry_t kernel_entry = (kernel_entry_t)(uintptr_t)boot_info->kernel_entry;
    kernel_entry(boot_info);

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }

    return EFI_SUCCESS;
}

