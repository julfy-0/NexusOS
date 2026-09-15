/* NexusOS PS/2 keyboard driver — i8042, translated Set-1 scancodes.
 *
 * Handles modifier state, Caps/Num/Scroll Lock LEDs, E0 extended keys and
 * normal ASCII input. The public shell/GUI API remains unchanged so the
 * input driver can be upgraded without rewriting the desktop.
 */
#include "keyboard.h"
#include "shell.h"
#include "gui.h"
#include "console.h"
#include "io.h"
#include "event_queue.h"
#include "input.h"

#define KBD_DATA_PORT    0x60
#define KBD_STATUS_PORT  0x64
#define KBD_CMD_PORT     0x64
#define KBD_STATUS_OBF   (1u << 0)
#define KBD_STATUS_IBF   (1u << 1)

#define SC_LSHIFT        0x2A
#define SC_RSHIFT        0x36
#define SC_LCTRL         0x1D
#define SC_LALT          0x38
#define SC_CAPS          0x3A
#define SC_NUM           0x45
#define SC_SCROLL         0x46
#define SC_EXTENDED      0xE0
#define SC_RELEASE       0x80

static volatile int g_shift;
static volatile int g_ctrl;
static volatile int g_alt;
static volatile int g_caps;
static volatile int g_num;
static volatile int g_scroll;
static volatile int g_present;
static uint8_t g_extended;

static const char scancode_ascii[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s',
    'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
    'b','n','m',',','.','/',0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const char scancode_shift[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S',
    'D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V',
    'B','N','M','<','>','?',0,'*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static void wait_input_clear(void) {
    for (uint32_t i = 0; i < 100000; ++i)
        if (!(inb(KBD_STATUS_PORT) & KBD_STATUS_IBF)) return;
}

static int wait_output_full(uint8_t *value) {
    for (uint32_t i = 0; i < 100000; ++i) {
        if (inb(KBD_STATUS_PORT) & KBD_STATUS_OBF) {
            *value = inb(KBD_DATA_PORT);
            return 1;
        }
    }
    return 0;
}

static void flush_output_buffer(void) {
    for (int i = 0; i < 32; ++i) {
        if (!(inb(KBD_STATUS_PORT) & KBD_STATUS_OBF)) break;
        (void)inb(KBD_DATA_PORT);
    }
}

static int keyboard_send(uint8_t command) {
    for (int retry = 0; retry < 3; ++retry) {
        wait_input_clear();
        outb(KBD_DATA_PORT, command);
        uint8_t reply = 0;
        if (!wait_output_full(&reply)) return 0;
        if (reply == 0xFA) return 1; /* ACK */
        if (reply != 0xFE) return 0; /* not RESEND */
    }
    return 0;
}

static void keyboard_set_leds(void) {
    uint8_t leds = (uint8_t)((g_scroll ? 1u : 0u) |
                             (g_num ? 2u : 0u) |
                             (g_caps ? 4u : 0u));
    if (!keyboard_send(0xED)) return;
    (void)keyboard_send(leds);
}

void keyboard_init(void) {
    g_present = 0;
    g_shift = g_ctrl = g_alt = 0;
    g_caps = g_num = g_scroll = 0;
    g_extended = 0;

    wait_input_clear(); outb(KBD_CMD_PORT, 0xAD); /* disable keyboard */
    wait_input_clear(); outb(KBD_CMD_PORT, 0xA7); /* disable mouse */
    flush_output_buffer();

    wait_input_clear(); outb(KBD_CMD_PORT, 0x20); /* read config */
    uint8_t config = 0;
    if (!wait_output_full(&config)) return;

    config |= 1u;                  /* IRQ1 */
    config &= (uint8_t)~(1u << 1);/* IRQ12 disabled until mouse init */
    config &= (uint8_t)~(1u << 4);/* keyboard clock enabled */
    config |= (1u << 6);           /* translate Set-2 -> Set-1 */

    wait_input_clear(); outb(KBD_CMD_PORT, 0x60);
    wait_input_clear(); outb(KBD_DATA_PORT, config);

    wait_input_clear(); outb(KBD_CMD_PORT, 0xAE); /* enable keyboard */
    flush_output_buffer();

    /* Enable scanning. */
    if (!keyboard_send(0xF4)) return;
    keyboard_set_leds();
    flush_output_buffer();
    g_present = 1;
    input_set_present(NEXUS_INPUT_SOURCE_PS2, NEXUS_INPUT_DEVICE_KEYBOARD, 1);
}

static void dispatch_special(uint8_t sc, int released) {
    if (released) return;
    if (!gui_is_active()) {
        if (sc == 0x49) console_scroll(1);       /* PgUp */
        else if (sc == 0x51) console_scroll(-1);/* PgDn */
        else if (sc == 0x48) shell_history_prev();
        else if (sc == 0x50) shell_history_next();
        return;
    }

    if (sc == 0x48) (void)gui_handle_key(GUI_KEY_UP);
    else if (sc == 0x50) (void)gui_handle_key(GUI_KEY_DOWN);
    else if (sc == 0x4B) (void)gui_handle_key(GUI_KEY_LEFT);
    else if (sc == 0x4D) (void)gui_handle_key(GUI_KEY_RIGHT);
}

void keyboard_process_scancode(uint8_t sc) {

    if (sc == SC_EXTENDED) {
        g_extended = 1;
        return;
    }

    int released = (sc & SC_RELEASE) != 0;
    sc &= 0x7F;

    if (g_extended) {
        g_extended = 0;
        /* Right Ctrl / Right Alt use E0 prefix. */
        if (sc == SC_LCTRL) { g_ctrl = !released; return; }
        if (sc == SC_LALT)  { g_alt = !released; return; }
        dispatch_special(sc, released);
        return;
    }

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) { g_shift = !released; return; }
    if (sc == SC_LCTRL) { g_ctrl = !released; return; }
    if (sc == SC_LALT)  { g_alt = !released; return; }

    if (sc == SC_CAPS && !released) { g_caps = !g_caps; keyboard_set_leds(); return; }
    if (sc == SC_NUM && !released)  { g_num = !g_num; keyboard_set_leds(); return; }
    if (sc == SC_SCROLL && !released) { g_scroll = !g_scroll; keyboard_set_leds(); return; }

    if (released || sc >= 128) return;

    char c = 0;
    if (sc < 128) {
        int alpha = (sc >= 0x10 && sc <= 0x19) ||
                    (sc >= 0x1E && sc <= 0x26) ||
                    (sc >= 0x2C && sc <= 0x32);
        if (alpha && g_caps)
            c = g_shift ? scancode_ascii[sc] : scancode_shift[sc];
        else
            c = g_shift ? scancode_shift[sc] : scancode_ascii[sc];
    }

    if (c == 0) return;

    input_record_keyboard(NEXUS_INPUT_SOURCE_PS2, (uint8_t)c);

    int was_gui = gui_is_active();
    if (was_gui) {
        (void)gui_handle_key(c);
        if (was_gui && !gui_is_active()) shell_return_from_desktop();
    } else {
        shell_input_char(c);
    }
}

void keyboard_handle_irq(void) {
    /* IRQ context: drain a small bounded burst, but do not touch shell/GUI
     * state. All interpretation happens in kernel_events_process(). */
    for (int i = 0; i < 16; ++i) {
        if (!(inb(KBD_STATUS_PORT) & KBD_STATUS_OBF)) break;
        (void)event_queue_push(NEXUS_EVENT_KEYBOARD_SCANCODE,
                               NEXUS_EVENT_SOURCE_KEYBOARD,
                               inb(KBD_DATA_PORT));
    }
}

int keyboard_is_present(void) { return g_present != 0; }

int keyboard_shift_down(void) { return g_shift != 0; }
int keyboard_ctrl_down(void) { return g_ctrl != 0; }
int keyboard_alt_down(void) { return g_alt != 0; }
int keyboard_caps_lock(void) { return g_caps != 0; }
int keyboard_num_lock(void) { return g_num != 0; }
int keyboard_scroll_lock(void) { return g_scroll != 0; }
