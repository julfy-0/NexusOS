/* NexusOS: минимальный PS/2 keyboard driver, scancode set 1, US QWERTY.
 * Достаточно для интерактивной демонстрации — печатаем то, что набрали. */
#include "keyboard.h"
#include "shell.h"
#include "console.h"
#include "pic.h"
#include "io.h"

#define KBD_DATA_PORT    0x60
#define KBD_STATUS_PORT  0x64
#define KBD_CMD_PORT     0x64

#define KBD_STATUS_OBF (1 << 0) /* output buffer full — есть байт для чтения */
#define KBD_STATUS_IBF (1 << 1) /* input buffer full — контроллер занят, писать нельзя */

#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_RELEASE_BIT 0x80

/* PgUp/PgDn (как и стрелки, Home/End и т.п.) — "extended" клавиши в
 * scancode set 1: контроллер шлёт их как ДВА байта, 0xE0 + собственно
 * код, а не один байт как у обычных клавиш. Наша таблица scancode_ascii[]
 * это не покрывает вообще (она рассчитана на однобайтовые make-коды),
 * поэтому раньше 0xE0 и следующий за ним байт просто терялись/трактовались
 * как непонятный код. */
#define SC_EXTENDED_PREFIX 0xE0
#define SC_PAGE_UP   0x49
#define SC_PAGE_DOWN 0x51

static int shift_down = 0;
static int extended_prefix = 0; /* только что пришёл 0xE0, следующий байт — extended-код */

/* Индекс — scancode (make code), значение — ASCII без Shift. 0 = игнорируем. */
static const char scancode_ascii[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,   'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,   '\\','z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};

static const char scancode_ascii_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,   'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0,   '|','Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
};

static void wait_input_clear(void) {
    /* Ждём, пока контроллер освободится, прежде чем писать команду/данные
     * в 0x64/0x60 — иначе можно "потерять" собственную же команду. */
    for (int i = 0; i < 100000; i++) {
        if (!(inb(KBD_STATUS_PORT) & KBD_STATUS_IBF)) return;
    }
}

static void wait_output_full(void) {
    for (int i = 0; i < 100000; i++) {
        if (inb(KBD_STATUS_PORT) & KBD_STATUS_OBF) return;
    }
}

static void flush_output_buffer(void) {
    /* Вычитываем и выбрасываем всё, что контроллер успел накопить до нас
     * (например, ещё во время работы прошивки) — иначе первые нажатия
     * могут потеряться или прийти как мусор вперемешку со старыми байтами. */
    for (int i = 0; i < 32; i++) {
        if (!(inb(KBD_STATUS_PORT) & KBD_STATUS_OBF)) break;
        (void)inb(KBD_DATA_PORT);
    }
}

void keyboard_init(void) {
    /* 1. Выключаем оба PS/2-порта на время настройки. */
    wait_input_clear();
    outb(KBD_CMD_PORT, 0xAD); /* disable first PS/2 port (клавиатура) */
    wait_input_clear();
    outb(KBD_CMD_PORT, 0xA7); /* disable second PS/2 port (мышь, если есть) */

    flush_output_buffer();

    /* 2. Читаем configuration byte контроллера и правим нужные биты:
     *    - бит0 (IRQ1 включён) = 1
     *    - бит1 (IRQ12 для мыши) = 0, мышь не используем
     *    - бит4 (clock первого порта отключён) = 0, т.е. порт активен
     *      (да, тут инвертированная логика — 0 значит "включено")
     *    - бит6 (трансляция scan code set 2 -> set 1) = 1 — без этого
     *      наша таблица scancode_ascii[] (которая ждёт set 1) может
     *      получать не те коды на контроллерах с другими настройками
     *      по умолчанию. */
    wait_input_clear();
    outb(KBD_CMD_PORT, 0x20); /* read controller configuration byte */
    wait_output_full();
    uint8_t config = inb(KBD_DATA_PORT);

    config |= (1 << 0);
    config &= (uint8_t)~(1 << 1);
    config &= (uint8_t)~(1 << 4);
    config |= (1 << 6);

    wait_input_clear();
    outb(KBD_CMD_PORT, 0x60); /* write controller configuration byte */
    wait_input_clear();
    outb(KBD_DATA_PORT, config);

    /* 3. Снова включаем порт клавиатуры. */
    wait_input_clear();
    outb(KBD_CMD_PORT, 0xAE); /* enable first PS/2 port */

    /* 4. На случай, если сама клавиатура была выключена — явно просим её
     *    начать сканирование. ACK (0xFA) не проверяем строго: часть
     *    контроллеров может не ответить вовремя, и это не повод считать
     *    инициализацию проваленной. */
    flush_output_buffer();
    wait_input_clear();
    outb(KBD_DATA_PORT, 0xF4); /* enable scanning */
    wait_output_full();
    (void)inb(KBD_DATA_PORT);

    flush_output_buffer();
}

void keyboard_handle_irq(void) {
    if (!(inb(KBD_STATUS_PORT) & KBD_STATUS_OBF)) {
        return; /* прерывание пришло, а данных нет — не читаем "в никуда" */
    }

    uint8_t sc = inb(KBD_DATA_PORT);

    if (sc == SC_EXTENDED_PREFIX) {
        extended_prefix = 1;
        return; /* сам префикс не клавиша, а флаг "следующий байт — extended" */
    }

    if (extended_prefix) {
        extended_prefix = 0;

        if (sc == SC_PAGE_UP) {
            /* Листаем назад (к старым строкам) на целый экран. */
            console_scroll((int32_t)console_get_rows());
        } else if (sc == SC_PAGE_DOWN) {
            /* Листаем вперёд (к живому выводу) на целый экран. */
            console_scroll(-(int32_t)console_get_rows());
        }
        /* Break-коды (сама клавиша | 0x80) и прочие extended-клавиши
         * (стрелки, Home/End, ...) пока осознанно игнорируем — не наша
         * задача сейчас, шелл всё равно однострочный. */
        return;
    }

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
        shift_down = 1;
        return;
    }
    if (sc == (SC_LSHIFT | SC_RELEASE_BIT) || sc == (SC_RSHIFT | SC_RELEASE_BIT)) {
        shift_down = 0;
        return;
    }
    if (sc & SC_RELEASE_BIT) {
        return; /* нас интересуют только нажатия, не отпускания */
    }
    if (sc >= 128) {
        return;
    }

    char c = shift_down ? scancode_ascii_shift[sc] : scancode_ascii[sc];
    if (c != 0) {
        shell_input_char(c);
    }
}
