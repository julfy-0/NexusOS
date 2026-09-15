#include "elf_loader.h"
#include "process.h"
#include "pmm.h"
#include "vmm.h"
#include "fat32.h"
#include <stdint.h>

#define ELF_BUFFER_SIZE 65536u
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define ET_EXEC 2
#define ET_DYN 3
#define EM_X86_64 62
#define PT_LOAD 1
#define PF_X 1
#define PF_W 2
#define PF_R 4
#define ELF_MAX_PHNUM 32
#define ELF_MAX_IMAGE_PAGES 12
#define ELF_STACK_PAGES 4
#define PAGE_SIZE 4096ULL
#define IMAGE_BASE 0x0000000000400000ULL
#define STACK_TOP PROCESS_USER_STACK_TOP

typedef struct {
    unsigned char ident[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} elf64_ehdr_t;

typedef struct {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, align;
} elf64_phdr_t;

static uint64_t align_down(uint64_t x) { return x & ~(PAGE_SIZE - 1ULL); }
static uint64_t align_up(uint64_t x) {
    if (x > UINT64_MAX - (PAGE_SIZE - 1ULL)) return 0;
    return (x + PAGE_SIZE - 1ULL) & ~(PAGE_SIZE - 1ULL);
}

static int range_ok(uint64_t start, uint64_t size, uint64_t limit) {
    return size <= limit - start;
}

static int phdr_bounds(const elf64_ehdr_t *eh, uint32_t file_size) {
    uint64_t bytes;
    if (!eh || eh->phentsize != sizeof(elf64_phdr_t) || eh->phnum == 0 || eh->phnum > ELF_MAX_PHNUM)
        return 0;
    bytes = (uint64_t)eh->phnum * eh->phentsize;
    return eh->phoff <= file_size && bytes <= (uint64_t)file_size - eh->phoff;
}

static int page_flags_for(const elf64_phdr_t *phdrs, uint16_t phnum, uint64_t va) {
    uint64_t page = align_down(va);
    int writable = 0, executable = 0;
    for (uint16_t i = 0; i < phnum; ++i) {
        const elf64_phdr_t *p = &phdrs[i];
        if (p->type != PT_LOAD || p->memsz == 0) continue;
        uint64_t begin = align_down(p->vaddr);
        uint64_t end = align_up(p->vaddr + p->memsz);
        if (end == 0 || page < begin || page >= end) continue;
        if (p->flags & PF_W) writable = 1;
        if (p->flags & PF_X) executable = 1;
    }
    uint64_t flags = 0;
    if (writable) flags |= VMM_PAGE_WRITABLE;
    if (!executable) flags |= VMM_PAGE_NX;
    return (int)flags;
}

int nexus_elf_load_process(uint64_t pid, const char *path) {
    static unsigned char file[ELF_BUFFER_SIZE];
    unsigned int file_size = 0;
    elf64_ehdr_t *eh = (elf64_ehdr_t *)file;
    elf64_phdr_t *phdrs;
    int64_t bias = 0;
    uint64_t image_start = UINT64_MAX, image_end = 0, entry;
    uint64_t stack_base = STACK_TOP - ELF_STACK_PAGES * PAGE_SIZE;
    uint64_t image_pages;
    int load_count = 0;

    nexus_process_t *proc = process_get(pid);
    if (!proc || proc->state == PROCESS_EXITED || proc->user_page_count != 0) return 0;
    if (!path || !fat32_read_file(path, file, sizeof(file), &file_size)) return 0;
    if (file_size < sizeof(elf64_ehdr_t)) return 0;
    if (eh->ident[0] != 0x7F || eh->ident[1] != 'E' || eh->ident[2] != 'L' || eh->ident[3] != 'F' ||
        eh->ident[4] != ELFCLASS64 || eh->ident[5] != ELFDATA2LSB || eh->machine != EM_X86_64 ||
        (eh->type != ET_EXEC && eh->type != ET_DYN) || !phdr_bounds(eh, file_size)) return 0;

    phdrs = (elf64_phdr_t *)(file + eh->phoff);
    for (uint16_t i = 0; i < eh->phnum; ++i) {
        elf64_phdr_t *p = &phdrs[i];
        if (p->type != PT_LOAD) continue;
        if (p->memsz == 0 || p->filesz > p->memsz || p->offset > file_size ||
            p->filesz > (uint64_t)file_size - p->offset) return 0;
        if (p->vaddr < PROCESS_USER_BASE || p->vaddr >= PROCESS_USER_LIMIT) return 0;
        if (!range_ok(p->vaddr, p->memsz, PROCESS_USER_LIMIT)) return 0;
        if (p->offset + p->filesz < p->offset) return 0;
        ++load_count;
        uint64_t begin = align_down(p->vaddr);
        uint64_t end = align_up(p->vaddr + p->memsz);
        if (!end || end <= begin) return 0;
        if (begin < image_start) image_start = begin;
        if (end > image_end) image_end = end;
    }
    if (load_count == 0 || image_start == UINT64_MAX || image_end <= image_start) return 0;

    if (eh->type == ET_DYN) {
        bias = (int64_t)IMAGE_BASE - (int64_t)image_start;
        if ((bias > 0 && image_end > UINT64_MAX - (uint64_t)bias) ||
            (bias < 0 && image_start < (uint64_t)(-bias))) return 0;
        image_start = (uint64_t)((int64_t)image_start + bias);
        image_end = (uint64_t)((int64_t)image_end + bias);
    }
    if (image_start < PROCESS_USER_BASE || image_end > PROCESS_USER_LIMIT || image_end <= image_start)
        return 0;
    image_pages = (image_end - image_start) / PAGE_SIZE;
    if (image_pages == 0 || image_pages > ELF_MAX_IMAGE_PAGES || image_pages + ELF_STACK_PAGES > PROCESS_USER_MAX_PAGES)
        return 0;
    if (image_end >= stack_base) return 0;

    /* Map the image with per-page permissions, then the user stack. */
    for (uint64_t va = image_start; va < image_end; va += PAGE_SIZE) {
        int flags = page_flags_for(phdrs, eh->phnum, (uint64_t)((int64_t)va - bias));
        if (!process_map_user_page(pid, va, (uint64_t)flags)) goto fail;
        if (!process_user_zero(pid, va, PAGE_SIZE)) goto fail;
    }
    for (uint64_t i = 0; i < ELF_STACK_PAGES; ++i) {
        uint64_t va = stack_base + i * PAGE_SIZE;
        if (!process_map_user_page(pid, va, VMM_PAGE_WRITABLE | VMM_PAGE_NX)) goto fail;
        if (!process_user_zero(pid, va, PAGE_SIZE)) goto fail;
    }

    /* Copy PT_LOAD file data and leave the remaining memsz as zero-filled BSS. */
    for (uint16_t i = 0; i < eh->phnum; ++i) {
        elf64_phdr_t *p = &phdrs[i];
        if (p->type != PT_LOAD || p->filesz == 0) continue;
        uint64_t dst = (uint64_t)((int64_t)p->vaddr + bias);
        if (!process_user_copy(pid, dst, file + p->offset, p->filesz)) goto fail;
    }

    entry = (uint64_t)((int64_t)eh->entry + bias);
    if (entry < image_start || entry >= image_end) goto fail;
    proc = process_get(pid);
    if (!proc) goto fail;
    proc->user_entry = entry;
    proc->user_stack_base = STACK_TOP;
    proc->user_stack_size = ELF_STACK_PAGES * PAGE_SIZE;
    proc->user_pages_reserved = proc->user_page_count;
    proc->flags |= PROCESS_FLAG_USER_CONTEXT;
    return 1;

fail:
    process_unmap_user_memory(pid);
    return 0;
}

uint64_t nexus_elf_user_stack_top(void) { return STACK_TOP; }
