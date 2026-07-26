#include "beep.h"
#include "console.h"
#include "io.h"

#define PIT_CHANNEL2_DATA 0x42
#define PIT_COMMAND        0x43
#define SPEAKER_PORT       0x61
#define PIT_BASE_FREQUENCY 1193182u

static void speaker_on(uint32_t freq) {
    uint32_t divisor = PIT_BASE_FREQUENCY / freq;
    outb(PIT_COMMAND, 0xB6); /* channel 2, lobyte/hibyte, mode 3, binary */
    outb(PIT_CHANNEL2_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL2_DATA, (uint8_t)((divisor >> 8) & 0xFF));

    uint8_t tmp = inb(SPEAKER_PORT);
    if (tmp != (tmp | 3)) {
        outb(SPEAKER_PORT, tmp | 3); /* включаем gate + data на спикер */
    }
}

static void speaker_off(void) {
    uint8_t tmp = inb(SPEAKER_PORT) & 0xFC;
    outb(SPEAKER_PORT, tmp);
}

void beep_run(void) {
    console_print("Beep!\n");

    speaker_on(1000); /* 1 кГц */

    /* Грубая программная задержка — своего sleep() с точным временем
     * пока нет смысла городить ради одного beep. */
    for (volatile long i = 0; i < 30000000; i++) {
        __asm__ volatile ("nop");
    }

    speaker_off();
}
