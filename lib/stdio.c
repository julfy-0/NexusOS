/*
 * NexusOS — реализация kprintf.
 * <stdarg.h> — часть freestanding-набора заголовков, которые
 * поставляются вместе с gcc (не зависят от libc хоста), поэтому
 * его использование не нарушает принцип «ядро без стандартной libc».
 */
#include "stdio.h"
#include "string.h"
#include <drivers/vga/vga.h>
#include <drivers/serial/serial.h>
#include <stdarg.h>

static void kputs(const char *s) {
    vga_puts(s);
    serial_puts(s);
}

static void kputchar(char c) {
    vga_putchar(c);
    serial_putchar(c);
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char numbuf[32];

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            kputchar(*p);
            continue;
        }

        p++;
        switch (*p) {
            case 'd': {
                int v = va_arg(args, int);
                itoa(v, numbuf, 10);
                kputs(numbuf);
                break;
            }
            case 'u': {
                unsigned int v = va_arg(args, unsigned int);
                utoa(v, numbuf, 10);
                kputs(numbuf);
                break;
            }
            case 'x': {
                unsigned int v = va_arg(args, unsigned int);
                utoa(v, numbuf, 16);
                kputs(numbuf);
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                kputs(s ? s : "(null)");
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                kputchar(c);
                break;
            }
            case '%': {
                kputchar('%');
                break;
            }
            default:
                kputchar('%');
                kputchar(*p);
                break;
        }
    }

    va_end(args);
}
