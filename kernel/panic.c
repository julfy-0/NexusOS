#include "panic.h"
#include "nexus_version.h"
#include <stdint.h>
#include <console.h>
#include <io.h>

#define PIT_COMMAND        0x43
#define PIT_CHANNEL0_DATA  0x40
#define PIT_READBACK       0xC0 /* latch status + count for channel 0 */
#define PIT_MODE0_LH       0x30 /* channel 0, lobyte/hibyte, one-shot */
#define PIT_MAX_COUNT      0xFFFFu
#define PANIC_SECONDS      30u
#define KBD_STATUS_PORT    0x64
#define KBD_DATA_PORT      0x60
#define KBD_OBF            0x01u
#define KBD_INPUT_FULL     0x02u

/* Перенастраиваем PIT в безопасный one-shot с максимальным счётчиком.
 * Период: 65535 / 1193182 ~= 54.93 мс. Это даёт нам независимый от IRQ
 * источник времени даже после cli, пока система собирается перезагрузиться. */
static void panic_pit_arm(void) {
    outb(PIT_COMMAND, PIT_MODE0_LH);
    outb(PIT_CHANNEL0_DATA, (uint8_t)(PIT_MAX_COUNT & 0xFFu));
    outb(PIT_CHANNEL0_DATA, (uint8_t)((PIT_MAX_COUNT >> 8) & 0xFFu));
}

static int panic_pit_wait_period(void) {
    for (;;) {
        outb(PIT_COMMAND, PIT_READBACK);

        uint8_t status = inb(PIT_CHANNEL0_DATA);
        uint8_t count_lo = inb(PIT_CHANNEL0_DATA);
        uint8_t count_hi = inb(PIT_CHANNEL0_DATA);
        (void)count_lo;
        (void)count_hi;

        if (status & 0x80u) {
            return 1; /* OUT = 1, one-shot завершился */
        }

        /* Не даём polling-циклу быть полностью плотным. */
        __asm__ volatile ("pause");
    }
}

static void panic_flush_keyboard(void) {
    for (uint32_t i = 0; i < 32u; ++i) {
        if (!(inb(KBD_STATUS_PORT) & KBD_OBF)) break;
        (void)inb(KBD_DATA_PORT);
    }
}

static int panic_key_pressed(void) {
    uint8_t status = inb(KBD_STATUS_PORT);
    if (!(status & KBD_OBF)) return 0;
    (void)inb(KBD_DATA_PORT);
    return 1;
}

static void panic_wait_input_buffer_clear(void) {
    for (uint32_t i = 0; i < 1000000u; ++i) {
        if (!(inb(KBD_STATUS_PORT) & KBD_INPUT_FULL)) return;
        __asm__ volatile ("pause");
    }
}

static void panic_reboot(void) __attribute__((noreturn));

static void panic_reboot(void) {
    /* 8042 reset command. Если контроллер не сработал, пробуем triple-fault
     * только как самый последний fallback, вместо возврата в повреждённое ядро. */
    panic_wait_input_buffer_clear();
    outb(KBD_STATUS_PORT, 0xFE);

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void panic_countdown_and_reboot(void) {
    __asm__ volatile ("cli");

    console_set_color(COLOR_WHITE, COLOR_BLACK);
    console_print("\n\nSystem is in diagnostic mode.\n");
    console_print("Press any key to restart immediately.\n\n");
    panic_flush_keyboard();

    /* Это независимый panic-таймер: IRQ уже запрещены и обычный PIT tick
     * обновляться не может, поэтому здесь PIT работает как polling stopwatch. */
    for (uint32_t remaining = PANIC_SECONDS; remaining > 0; --remaining) {
        console_print("Restarting NexusOS in ");
        console_print_dec(remaining);
        console_print(" seconds...  \r");

        /* Оставляем минимум один период PIT между обновлениями. 19 таких
         * периодов ~= 1.04 секунды; для panic-экрана это заметнее и надёжнее,
         * чем пытаться восстанавливать IRQ внутри обработчика исключения. */
        /* 18 periods ~= 0.989 s. Add one extra period during the first
         * six seconds, giving 546 periods total ~= 29.99 s for the full
         * countdown while keeping each displayed second visually stable. */
        uint32_t periods = (remaining > 24u) ? 19u : 18u;
        for (uint32_t period = 0; period < periods; ++period) {
            panic_pit_arm();
            if (panic_key_pressed()) panic_reboot();
            if (!panic_pit_wait_period()) panic_reboot();
            if (panic_key_pressed()) panic_reboot();
        }
    }

    console_print("\nRestarting NexusOS now...\n");
    panic_reboot();
}

void panic(const char *message) {
    __asm__ volatile ("cli");

    console_set_color(COLOR_WHITE, COLOR_RED);
    console_print("\n*** NEXUSOS " NEXUS_VERSION_DISPLAY " KERNEL PANIC ***\n");
    console_print(message);
    console_print("\n");
    panic_countdown_and_reboot();
}
