/* NexusOS PS/2 mouse driver.
 *
 * Features:
 *  - i8042 auxiliary-port setup and IRQ12 support
 *  - automatic mouse identification
 *  - standard 3-byte packets
 *  - IntelliMouse-compatible 4-byte wheel packets
 *  - signed movement with overflow checking
 *  - left/right/middle/extra buttons
 *  - screen-coordinate clamping for the GUI
 */
#include "mouse.h"
#include "io.h"
#include "event_queue.h"
#include "input.h"

#define PS2_DATA       0x60
#define PS2_STATUS     0x64
#define PS2_CMD        0x64

#define ST_OBF         0x01
#define ST_IBF         0x02
#define ST_AUX         0x20

#define MOUSE_ACK      0xFA
#define MOUSE_RESEND   0xFE
#define MOUSE_ID_STD   0x00
#define MOUSE_ID_WHEEL 0x03

static volatile int g_present;
static volatile int g_moved;
static volatile int32_t g_x;
static volatile int32_t g_y;
static volatile uint8_t g_buttons;
static volatile int8_t g_wheel;
static uint32_t g_w = 1024;
static uint32_t g_h = 768;
static uint8_t g_packet[4];
static uint8_t g_packet_index;
static uint8_t g_packet_len = 3;
static uint8_t g_device_id = MOUSE_ID_STD;

int mouse_process_byte(uint8_t b);

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

static void flush_output_buffer(void) {
    for (int i = 0; i < 32; ++i) {
        if (!(inb(PS2_STATUS) & ST_OBF)) break;
        (void)inb(PS2_DATA);
    }
}

static int mouse_write_byte(uint8_t value) {
    for (int retry = 0; retry < 3; ++retry) {
        wait_write();
        outb(PS2_CMD, 0xD4); /* following byte goes to auxiliary device */
        wait_write();
        outb(PS2_DATA, value);

        uint8_t reply = 0;
        if (!wait_read(&reply)) return 0;
        if (reply == MOUSE_ACK) return 1;
        if (reply != MOUSE_RESEND) return 0;
    }
    return 0;
}

static int mouse_command_with_reply(uint8_t command, uint8_t *reply) {
    wait_write();
    outb(PS2_CMD, 0xD4);
    wait_write();
    outb(PS2_DATA, command);
    return wait_read(reply);
}

static int mouse_set_sample_rate(uint8_t rate) {
    if (!mouse_write_byte(0xF3)) return 0;
    return mouse_write_byte(rate);
}

static int mouse_identify(uint8_t *id) {
    uint8_t reply = 0;
    if (!mouse_command_with_reply(0xF2, &reply)) return 0; /* ACK */
    if (reply != MOUSE_ACK) return 0;
    if (!wait_read(id)) return 0;
    return 1;
}

static int32_t clamp_coord(int64_t value, uint32_t limit) {
    if (limit <= 1) return 0;
    if (value < 0) return 0;
    if (value >= (int64_t)limit) return (int32_t)limit - 1;
    return (int32_t)value;
}

void mouse_init(void) {
    g_present = 0;
    g_moved = 0;
    g_packet_index = 0;
    g_packet_len = 3;
    g_device_id = MOUSE_ID_STD;
    g_buttons = 0;
    g_wheel = 0;
    g_x = 0;
    g_y = 0;

    /* Disable auxiliary port while changing controller configuration. */
    wait_write();
    outb(PS2_CMD, 0xA7);
    flush_output_buffer();

    /* Read i8042 configuration byte. */
    wait_write();
    outb(PS2_CMD, 0x20);
    uint8_t config = 0;
    if (!wait_read(&config)) return;

    /* IRQ12 on, auxiliary clock on. Preserve unrelated controller bits. */
    config |= (1u << 1);
    config &= (uint8_t)~(1u << 5);
    wait_write();
    outb(PS2_CMD, 0x60);
    wait_write();
    outb(PS2_DATA, config);

    /* Enable auxiliary port. */
    wait_write();
    outb(PS2_CMD, 0xA8);
    flush_output_buffer();

    /* Return the mouse to a known 1:1, 100 Hz, 4 counts/mm-ish default. */
    if (!mouse_write_byte(0xF6)) return; /* SET DEFAULTS */
    (void)mouse_write_byte(0xE6);        /* SET SCALING 1:1 */
    (void)mouse_write_byte(0xE8);         /* SET RESOLUTION */
    (void)mouse_write_byte(0x02);         /* 4 counts/mm */
    (void)mouse_set_sample_rate(100);

    /* IntelliMouse detection sequence: 200, 100, 80 -> ID 03 on
     * wheel-capable devices. A normal mouse remains ID 00. */
    (void)mouse_set_sample_rate(200);
    (void)mouse_set_sample_rate(100);
    (void)mouse_set_sample_rate(80);

    uint8_t id = MOUSE_ID_STD;
    if (mouse_identify(&id)) {
        g_device_id = id;
        if (id == MOUSE_ID_WHEEL) g_packet_len = 4;
    }

    /* Enable data reporting. */
    if (!mouse_write_byte(0xF4)) return;

    g_present = 1;
    input_set_present(NEXUS_INPUT_SOURCE_PS2, NEXUS_INPUT_DEVICE_MOUSE, 1);
    g_x = (int32_t)(g_w / 2u);
    g_y = (int32_t)(g_h / 2u);
    g_packet_index = 0;
    flush_output_buffer();
}

void mouse_set_screen_size(uint32_t width, uint32_t height) {
    if (width == 0) width = 1;
    if (height == 0) height = 1;
    g_w = width;
    g_h = height;
    g_x = clamp_coord(g_x, g_w);
    g_y = clamp_coord(g_y, g_h);
}

int mouse_process_byte(uint8_t b) {
    /* First byte always has bit 3 set. This resynchronizes the stream after
     * dropped bytes or an unexpected controller response. */
    if (g_packet_index == 0) {
        if (!(b & 0x08)) return 0;
        g_packet[0] = b;
        g_packet_index = 1;
        return 0;
    }

    g_packet[g_packet_index++] = b;
    if (g_packet_index != g_packet_len) return 0;
    g_packet_index = 0;

    uint8_t status = g_packet[0];
    if (status & 0xC0) return 0; /* X/Y overflow */

    int8_t dx = (int8_t)g_packet[1];
    int8_t dy = (int8_t)g_packet[2];
    uint8_t new_buttons = status & 0x07;

    /* IntelliMouse extra byte: low nibble is signed wheel movement. */
    if (g_packet_len == 4) {
        int8_t wheel = (int8_t)g_packet[3];
        /* Wheel devices normally use a signed nibble in a signed byte. */
        wheel = (int8_t)(wheel & 0x0F);
        if (wheel & 0x08) wheel = (int8_t)(wheel | 0xF0);
        if (wheel != 0) g_wheel = (int8_t)(g_wheel + wheel);
        new_buttons |= (uint8_t)(g_packet[3] & 0x30); /* extra buttons */
    }

    int32_t nx = clamp_coord((int64_t)g_x + dx, g_w);
    int32_t ny = clamp_coord((int64_t)g_y - dy, g_h);

    if (nx != g_x || ny != g_y || g_buttons != new_buttons) g_moved = 1;
    g_x = nx;
    g_y = ny;
    g_buttons = new_buttons;
    input_record_mouse(NEXUS_INPUT_SOURCE_PS2, dx, -dy, new_buttons, g_wheel);
    return 1;
}

void mouse_handle_irq(void) {
    /* IRQ context only captures raw auxiliary bytes. Packet assembly and GUI
     * state updates happen in kernel_events_process(). */
    for (int i = 0; i < 16; ++i) {
        uint8_t status = inb(PS2_STATUS);
        if (!(status & ST_OBF) || !(status & ST_AUX)) break;
        (void)event_queue_push(NEXUS_EVENT_MOUSE_BYTE,
                               NEXUS_EVENT_SOURCE_MOUSE,
                               inb(PS2_DATA));
    }
}

int mouse_poll(void) {
    int packets = 0;
    for (int i = 0; i < 8; ++i) {
        uint8_t status = inb(PS2_STATUS);
        if (!(status & ST_OBF) || !(status & ST_AUX)) break;
        packets |= mouse_process_byte(inb(PS2_DATA));
    }
    return packets;
}

int mouse_is_present(void) { return g_present; }
int mouse_has_moved(void) { return g_moved != 0; }
int32_t mouse_get_x(void) { return g_x; }
int32_t mouse_get_y(void) { return g_y; }
uint8_t mouse_get_buttons(void) { return g_buttons; }
int8_t mouse_get_wheel(void) { return g_wheel; }
int mouse_has_wheel(void) { return g_packet_len == 4; }
void mouse_clear_moved(void) { g_moved = 0; }
void mouse_clear_wheel(void) { g_wheel = 0; }

void mouse_process_usb_report(const uint8_t *report, uint8_t length) {
    if (!report || length < 3) return;
    uint8_t buttons = report[0] & 0x07;
    int8_t dx = (int8_t)report[1];
    int8_t dy = (int8_t)report[2];
    int8_t wheel = length >= 4 ? (int8_t)report[3] : 0;
    int32_t nx = clamp_coord((int64_t)g_x + dx, g_w);
    int32_t ny = clamp_coord((int64_t)g_y - dy, g_h);
    if (nx != g_x || ny != g_y || buttons != g_buttons) g_moved = 1;
    g_x = nx; g_y = ny; g_buttons = buttons;
    if (wheel) g_wheel = (int8_t)(g_wheel + wheel);
    g_present = 1;
    input_set_present(NEXUS_INPUT_SOURCE_USB, NEXUS_INPUT_DEVICE_MOUSE, 1);
    input_record_mouse(NEXUS_INPUT_SOURCE_USB, dx, -dy, buttons, wheel);
}
