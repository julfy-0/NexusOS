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

/* Парсит ELF64, раскладывает PT_LOAD сегменты по их физическим адресам.
 * Возвращает точку входа. */
static uint64_t load_elf(void *elf_data) {
    Elf64_Ehdr *eh = (Elf64_Ehdr *)elf_data;

    if (eh->e_ident[0] != ELF_MAGIC0 || eh->e_ident[1] != ELF_MAGIC1 ||
        eh->e_ident[2] != ELF_MAGIC2 || eh->e_ident[3] != ELF_MAGIC3) {
        panic(u"kernel.elf: bad magic");
    }
    if (eh->e_ident[4] != ELFCLASS64) {
        panic(u"kernel.elf: not 64-bit");
    }
    if (eh->e_machine != EM_X86_64) {
        panic(u"kernel.elf: not x86_64");
    }

    Elf64_Phdr *phdrs = (Elf64_Phdr *)((uint8_t *)elf_data + eh->e_phoff);

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        Elf64_Phdr *ph = &phdrs[i];
        if (ph->p_type != PT_LOAD) continue;

        UINTN pages = (ph->p_memsz + 4095) / 4096;
        EFI_PHYSICAL_ADDRESS addr = ph->p_paddr;

        EFI_STATUS status = g_bs->AllocatePages(AllocateAddress, EfiLoaderData, pages, &addr);
        if (EFI_ERROR(status)) {
            panic(u"AllocatePages for segment failed");
        }

        /* Копируем данные сегмента из файла и обнуляем .bss-хвост */
        memcpy((void *)ph->p_paddr, (uint8_t *)elf_data + ph->p_offset, ph->p_filesz);
        if (ph->p_memsz > ph->p_filesz) {
            memset((void *)(ph->p_paddr + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);
        }
    }

    return eh->e_entry;
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
            if (ascii_copy_smbios_string(strings, end, manufacturer,
                                          bi->system_manufacturer, sizeof(bi->system_manufacturer)) ||
                ascii_copy_smbios_string(strings, end, product,
                                          bi->system_product, sizeof(bi->system_product))) {
                bi->system_info_valid = 1;
            }
            return;
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


    static nexus_boot_info_t boot_info;
    memset(&boot_info, 0, sizeof(boot_info));
    boot_info.magic = NEXUS_BOOT_MAGIC;
    detect_system_identity(&boot_info);
    detect_acpi(&boot_info);

    /* ---- 1. Видеорежим через GOP ---- */
    EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    EFI_STATUS status = g_bs->LocateProtocol(&gop_guid, NULL, (void **)&gop);
    if (EFI_ERROR(status) || gop == NULL) {
        panic(u"GOP not found - cannot continue without a framebuffer");
    }

    boot_info.fb.base = gop->Mode->FrameBufferBase;
    boot_info.fb.size = gop->Mode->FrameBufferSize;
    boot_info.fb.width = gop->Mode->Info->HorizontalResolution;
    boot_info.fb.height = gop->Mode->Info->VerticalResolution;
    boot_info.fb.pixels_per_scanline = gop->Mode->Info->PixelsPerScanLine;

    if (gop->Mode->Info->PixelFormat == PixelRedGreenBlueReserved8BitPerColor) {
        boot_info.fb.pixel_format = NEXUS_PIXFMT_RGB;
    } else if (gop->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
        boot_info.fb.pixel_format = NEXUS_PIXFMT_BGR;
    } else {
        boot_info.fb.pixel_format = NEXUS_PIXFMT_OTHER;
    }

    if (boot_info.fb.pixel_format == NEXUS_PIXFMT_OTHER) {
        panic(u"Unsupported framebuffer pixel format");
    }

    boot_graphics_init(gop);
    boot_progress(1, 7, u"");

    boot_progress(2, 7, u"Opening boot volume");

    /* ---- 2. Открываем том, с которого загрузились, и читаем kernel.elf ---- */
    EFI_GUID loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;
    status = g_bs->HandleProtocol(ImageHandle, &loaded_image_guid, (void **)&loaded_image);
    if (EFI_ERROR(status)) {
        panic(u"cannot get LoadedImageProtocol");
    }

    EFI_GUID sfs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs = NULL;
    status = g_bs->HandleProtocol(loaded_image->DeviceHandle, &sfs_guid, (void **)&sfs);
    if (EFI_ERROR(status)) {
        panic(u"cannot get SimpleFileSystemProtocol");
    }

    EFI_FILE_PROTOCOL *root = NULL;
    status = sfs->OpenVolume(sfs, &root);
    if (EFI_ERROR(status)) {
        panic(u"OpenVolume failed");
    }

    boot_progress(3, 7, u"Loading kernel image");
    UINTN kernel_size;
    void *kernel_data = load_file(root, u"\\kernel.elf", &kernel_size);

    boot_progress(4, 7, u"Preparing kernel memory");
    uint64_t entry_point = load_elf(kernel_data);

    boot_progress(5, 7, u"Finalizing memory map");

    /* ---- 3. Финальная memory map + ExitBootServices ---- */
    UINTN mmap_size = 0;
    EFI_MEMORY_DESCRIPTOR *mmap = NULL;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    uint32_t desc_version = 0;

    /* Первый вызов — узнаём нужный размер буфера */
    g_bs->GetMemoryMap(&mmap_size, mmap, &map_key, &desc_size, &desc_version);
    mmap_size += desc_size * 8; /* запас: сама аллокация буфера меняет карту */
    g_bs->AllocatePool(EfiLoaderData, mmap_size, (void **)&mmap);

    status = g_bs->GetMemoryMap(&mmap_size, mmap, &map_key, &desc_size, &desc_version);
    if (EFI_ERROR(status)) {
        panic(u"GetMemoryMap (final) failed");
    }

    boot_info.mmap.map_base = (uint64_t)(uintptr_t)mmap;
    boot_info.mmap.map_size = mmap_size;
    boot_info.mmap.descriptor_size = desc_size;
    boot_info.mmap.descriptor_version = desc_version;

    boot_progress(7, 7, u"Starting NexusOS 0.5.1 - Desktop Update");
    status = g_bs->ExitBootServices(ImageHandle, map_key);
    if (EFI_ERROR(status)) {
        /* Карта могла устареть между вызовами (это нормально по спеке) —
         * пробуем ещё раз с самого начала. */
        mmap_size = boot_info.mmap.map_size + desc_size * 8;
        status = g_bs->GetMemoryMap(&mmap_size, mmap, &map_key, &desc_size, &desc_version);
        status = g_bs->ExitBootServices(ImageHandle, map_key);
        if (EFI_ERROR(status)) {
            panic(u"ExitBootServices failed twice");
        }
    }

    /* С этого момента печатать через ConOut больше нельзя — Boot Services мертвы. */

    typedef void (*kernel_entry_t)(nexus_boot_info_t *);
    kernel_entry_t kernel_entry = (kernel_entry_t)entry_point;
    kernel_entry(&boot_info);

    /* Ядро не должно возвращаться сюда. Если вернулось — зависаем. */
    for (;;) {
        __asm__ volatile("hlt");
    }

    return EFI_SUCCESS;
}
