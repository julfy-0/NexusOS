/* NexusOS PS/2 mouse driver (auxiliary port, 3-byte packets). */
#include "mouse.h"
#include "io.h"

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

#define ST_OBF 0x01
#define ST_IBF 0x02

static volatile int g_present;
static volatile int g_moved;
static volatile int32_t g_x = 0;
static volatile int32_t g_y = 0;
static volatile uint8_t g_buttons = 0;
static uint32_t g_w = 1024;
static uint32_t g_h = 768;
static uint8_t g_packet[3];
static uint8_t g_packet_index;

static int mouse_process_byte(uint8_t b);

static void wait_write(void) {
    for (uint32_t i = 0; i < 100000; ++i) {
        if (!(inb(PS2_STATUS) & ST_IBF)) return;
    }
}

static int wait_read(uint8_t *value) {
    for (uint32_t i = 0; i < 100000; ++i) {
        if (inb(PS2_STATUS) & ST_OBF) {
            *value = inb(PS2_DATA);
            return 1;
        }
    }
    return 0;
}

static void flush_mouse_data(void) {
    for (int i = 0; i < 32; ++i) {
        if (!(inb(PS2_STATUS) & ST_OBF)) break;
        (void)inb(PS2_DATA);
    }
}

static int mouse_write(uint8_t value) {
    wait_write();
    outb(PS2_CMD, 0xD4); /* next byte goes to auxiliary device */
    wait_write();
    outb(PS2_DATA, value);
    uint8_t ack = 0;
    if (!wait_read(&ack)) return 0;
    return ack == 0xFA;
}

void mouse_init(void) {
    g_present = 0;
    g_moved = 0;
    g_packet_index = 0;
    g_buttons = 0;
    g_x = 0;
    g_y = 0;

    /* Disable auxiliary port while configuring it. */
    wait_write();
    outb(PS2_CMD, 0xA7);
    flush_mouse_data();

    /* Read controller configuration byte. */
    wait_write();
    outb(PS2_CMD, 0x20);
    uint8_t config;
    if (!wait_read(&config)) return;

    /* Enable IRQ12 and auxiliary clock; preserve translation/other bits. */
    config |= (1u << 1);  /* IRQ12 enabled */
    config &= (uint8_t)~(1u << 5); /* auxiliary clock enabled */
    wait_write();
    outb(PS2_CMD, 0x60);
    wait_write();
    outb(PS2_DATA, config);

    /* Enable auxiliary port and turn mouse reporting on. */
    wait_write();
    outb(PS2_CMD, 0xA8);
    flush_mouse_data();

    if (!mouse_write(0xF4)) {
        /* No responding mouse is fine; don't make boot fail. */
        g_present = 0;
        return;
    }

    g_present = 1;
    g_x = (int32_t)(g_w / 2u);
    g_y = (int32_t)(g_h / 2u);
}

void mouse_set_screen_size(uint32_t width, uint32_t height) {
    if (width == 0) width = 1;
    if (height == 0) height = 1;
    g_w = width;
    g_h = height;
    if (g_x >= (int32_t)g_w) g_x = (int32_t)g_w - 1;
    if (g_y >= (int32_t)g_h) g_y = (int32_t)g_h - 1;
}

void mouse_handle_irq(void) {
    if (!(inb(PS2_STATUS) & ST_OBF)) return;
    /* IRQ12 must correspond to auxiliary data. */
    if (!(inb(PS2_STATUS) & (1u << 5))) return;
    (void)mouse_process_byte(inb(PS2_DATA));
}


static int mouse_process_byte(uint8_t b) {
    if (g_packet_index == 0) {
        if (!(b & 0x08)) return 0;
        g_packet[0] = b;
        g_packet_index = 1;
        return 0;
    }

    g_packet[g_packet_index++] = b;
    if (g_packet_index != 3) return 0;
    g_packet_index = 0;

    int8_t dx = (int8_t)g_packet[1];
    int8_t dy = (int8_t)g_packet[2];
    uint8_t status = g_packet[0];
    if (status & 0xC0) return 0;

    uint8_t new_buttons = status & 0x07;
    int32_t nx = g_x + (int32_t)dx;
    int32_t ny = g_y - (int32_t)dy;

    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx >= (int32_t)g_w) nx = (int32_t)g_w - 1;
    if (ny >= (int32_t)g_h) ny = (int32_t)g_h - 1;

    if (nx != g_x || ny != g_y || g_buttons != new_buttons) g_moved = 1;
    g_x = nx;
    g_y = ny;
    g_buttons = new_buttons;
    return 1;
}

int mouse_poll(void) {
    int packets = 0;
    /* Status bit 5 is set for auxiliary (mouse) data on a standard i8042. */
    for (int i = 0; i < 8; ++i) {
        uint8_t status = inb(PS2_STATUS);
        if (!(status & ST_OBF) || !(status & (1u << 5))) break;
        uint8_t b = inb(PS2_DATA);
        packets |= mouse_process_byte(b);
    }
    return packets;
}

int mouse_is_present(void) { return g_present; }
int mouse_has_moved(void) { return g_moved != 0; }
int32_t mouse_get_x(void) { return g_x; }
int32_t mouse_get_y(void) { return g_y; }
uint8_t mouse_get_buttons(void) { return g_buttons; }
void mouse_clear_moved(void) { g_moved = 0; }
