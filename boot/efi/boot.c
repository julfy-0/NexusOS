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

extern void *memcpy(void *dest, const void *src, unsigned long n);
extern void *memset(void *dest, int value, unsigned long n);

static EFI_SYSTEM_TABLE *g_st;
static EFI_BOOT_SERVICES *g_bs;

static void print(CHAR16 *str) {
    g_st->ConOut->OutputString(g_st->ConOut, str);
}

static void print_hex(uint64_t value) {
    CHAR16 buf[19];
    const CHAR16 *digits = u"0123456789ABCDEF";
    buf[0] = u'0';
    buf[1] = u'x';
    buf[18] = 0;
    for (int i = 0; i < 16; i++) {
        buf[17 - i] = digits[value & 0xF];
        value >>= 4;
    }
    print(buf);
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

EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    g_st = SystemTable;
    g_bs = SystemTable->BootServices;

    g_st->ConOut->ClearScreen(g_st->ConOut);
    g_st->ConOut->SetAttribute(g_st->ConOut, EFI_LIGHTGREEN | EFI_BACKGROUND_BLACK);
    print(u"NexusOS Bootloader v0.2\r\n");
    g_st->ConOut->SetAttribute(g_st->ConOut, EFI_WHITE | EFI_BACKGROUND_BLACK);
    print(u"========================\r\n\r\n");

    static nexus_boot_info_t boot_info;
    memset(&boot_info, 0, sizeof(boot_info));
    boot_info.magic = NEXUS_BOOT_MAGIC;

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

    print(u"Framebuffer: ");
    print_hex(boot_info.fb.width);
    print(u" x ");
    print_hex(boot_info.fb.height);
    print(u" @ ");
    print_hex(boot_info.fb.base);
    print(u"\r\n");

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

    print(u"Loading \\kernel.elf ...\r\n");
    UINTN kernel_size;
    void *kernel_data = load_file(root, u"\\kernel.elf", &kernel_size);
    print(u"Kernel file loaded, size = ");
    print_hex(kernel_size);
    print(u" bytes\r\n");

    uint64_t entry_point = load_elf(kernel_data);
    print(u"Kernel entry point: ");
    print_hex(entry_point);
    print(u"\r\n");

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

    print(u"Exiting boot services...\r\n");
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
