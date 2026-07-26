/*
 * NexusOS — драйвер PS/2 клавиатуры, набор Scan Code Set 1.
 * Подключается к IRQ1 через arch/i386/irq.c. Здесь реализована
 * только базовая раскладка US QWERTY без модификаторов (Shift/Ctrl
 * оставлены как задел на будущее — легко добавить в keyboard_callback).
 */
#include "keyboard.h"
#include <arch/i386/io.h>
#include <arch/i386/irq.h>
#include <nexus/types.h>

#define KBD_DATA_PORT 0x60

static volatile char last_char = 0;

/* Таблица переводит scan code (Set 1, "make" коды) в ASCII.
 * Индекс — сам scan code, значение 0 = не печатаемый/не поддержан. */
static const char scancode_to_ascii[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0,
    /* остальное (F-клавиши, стрелки, numpad и т.п.) — не отображаем */
};

static void keyboard_callback(registers_t *regs) {
    NX_UNUSED(regs);
    u8 scancode = inb(KBD_DATA_PORT);

    /* Бит 0x80 в scan code означает "клавиша отпущена" — игнорируем */
    if (scancode & 0x80) {
        return;
    }

    if (scancode < 128) {
        char c = scancode_to_ascii[scancode];
        if (c != 0) {
            last_char = c;
        }
    }
}

void keyboard_init(void) {
    irq_install_handler(1, keyboard_callback);
}

char keyboard_getchar(void) {
    char c = last_char;
    last_char = 0;
    return c;
}
