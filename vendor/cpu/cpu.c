/* NexusOS: минимальный доступ к CPUID. */
#include "cpu.h"

static inline void cpuid(uint32_t leaf, uint32_t subleaf,
                          uint32_t *a, uint32_t *b, uint32_t *c, uint32_t *d) {
    __asm__ volatile ("cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf), "c"(subleaf));
}

void cpu_get_vendor(char *out) {
    uint32_t a, b, c, d;
    cpuid(0, 0, &a, &b, &c, &d);
    /* Порядок регистров для строки вендора именно EBX,EDX,ECX — это не опечатка,
     * так исторически определено в CPUID leaf 0. */
    out[0]  = (char)(b & 0xFF);
    out[1]  = (char)((b >> 8) & 0xFF);
    out[2]  = (char)((b >> 16) & 0xFF);
    out[3]  = (char)((b >> 24) & 0xFF);
    out[4]  = (char)(d & 0xFF);
    out[5]  = (char)((d >> 8) & 0xFF);
    out[6]  = (char)((d >> 16) & 0xFF);
    out[7]  = (char)((d >> 24) & 0xFF);
    out[8]  = (char)(c & 0xFF);
    out[9]  = (char)((c >> 8) & 0xFF);
    out[10] = (char)((c >> 16) & 0xFF);
    out[11] = (char)((c >> 24) & 0xFF);
    out[12] = '\0';
}

int cpu_has_brand_string(void) {
    uint32_t a, b, c, d;
    cpuid(0x80000000, 0, &a, &b, &c, &d);
    return a >= 0x80000004;
}

void cpu_get_brand(char *out) {
    uint32_t regs[12];
    for (int i = 0; i < 3; i++) {
        cpuid(0x80000002 + (uint32_t)i, 0,
              &regs[i * 4 + 0], &regs[i * 4 + 1], &regs[i * 4 + 2], &regs[i * 4 + 3]);
    }
    uint8_t *src = (uint8_t *)regs;
    for (int i = 0; i < 48; i++) out[i] = (char)src[i];
    out[48] = '\0';
}

uint32_t cpu_logical_cores(void) {
    uint32_t a, b, c, d;
    cpuid(1, 0, &a, &b, &c, &d);
    uint32_t count = (b >> 16) & 0xFF;
    return count == 0 ? 1 : count;
}
