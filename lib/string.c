/*
 * NexusOS — минимальная freestanding-реализация части libc,
 * необходимой ядру. Ядро не может линковаться с libc хоста,
 * поэтому все эти функции пишем сами.
 */
#include "string.h"

void *memset(void *dest, int value, size_t count) {
    u8 *p = (u8 *)dest;
    for (size_t i = 0; i < count; i++) {
        p[i] = (u8)value;
    }
    return dest;
}

void *memcpy(void *dest, const void *src, size_t count) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    for (size_t i = 0; i < count; i++) {
        d[i] = s[i];
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t count) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    if (d < s) {
        for (size_t i = 0; i < count; i++) d[i] = s[i];
    } else {
        for (size_t i = count; i > 0; i--) d[i - 1] = s[i - 1];
    }
    return dest;
}

int memcmp(const void *a, const void *b, size_t count) {
    const u8 *pa = (const u8 *)a;
    const u8 *pb = (const u8 *)b;
    for (size_t i = 0; i < count; i++) {
        if (pa[i] != pb[i]) return pa[i] - pb[i];
    }
    return 0;
}

size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

char *strcpy(char *dest, const char *src) {
    char *ret = dest;
    while ((*dest++ = *src++));
    return ret;
}

char *strcat(char *dest, const char *src) {
    char *ret = dest;
    while (*dest) dest++;
    while ((*dest++ = *src++));
    return ret;
}

void utoa(unsigned int value, char *buf, int base) {
    static const char digits[] = "0123456789abcdef";
    char tmp[32];
    int i = 0;

    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (value != 0) {
        tmp[i++] = digits[value % base];
        value /= base;
    }

    int j = 0;
    while (i > 0) {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

void itoa(int value, char *buf, int base) {
    if (value < 0 && base == 10) {
        buf[0] = '-';
        utoa((unsigned int)(-value), buf + 1, base);
    } else {
        utoa((unsigned int)value, buf, base);
    }
}
