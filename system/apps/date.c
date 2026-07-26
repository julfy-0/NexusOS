/* NexusOS: чтение даты/времени из CMOS Real-Time Clock.
 * Без коррекции часового пояса — печатаем то, что храним в CMOS как есть
 * (обычно UTC или локальное время BIOS, зависит от настройки в самой
 * прошивке/BIOS Setup). */
#include "date.h"
#include "console.h"
#include "io.h"

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

static int rtc_update_in_progress(void) {
    outb(CMOS_ADDRESS, 0x0A);
    return inb(CMOS_DATA) & 0x80;
}

static uint8_t bcd_to_bin(uint8_t v) {
    return (uint8_t)((v & 0x0F) + ((v / 16) * 10));
}

static void print_2digit(uint8_t v) {
    console_putchar((char)('0' + (v / 10) % 10));
    console_putchar((char)('0' + (v % 10)));
}

void date_run(void) {
    /* Ждём, пока RTC не занят обновлением (бит 7 регистра A) — иначе
     * можно прочитать значение прямо во время его смены. */
    for (int i = 0; i < 1000000 && rtc_update_in_progress(); i++) { }

    uint8_t second = cmos_read(0x00);
    uint8_t minute = cmos_read(0x02);
    uint8_t hour   = cmos_read(0x04);
    uint8_t day    = cmos_read(0x07);
    uint8_t month  = cmos_read(0x08);
    uint8_t year   = cmos_read(0x09);
    uint8_t status_b = cmos_read(0x0B);

    if (!(status_b & 0x04)) { /* бит2=0 -> данные в BCD, нужно перевести */
        second = bcd_to_bin(second);
        minute = bcd_to_bin(minute);
        hour   = (uint8_t)(bcd_to_bin(hour & 0x7F) | (hour & 0x80));
        day    = bcd_to_bin(day);
        month  = bcd_to_bin(month);
        year   = bcd_to_bin(year);
    }

    if (!(status_b & 0x02) && (hour & 0x80)) {
        /* 12-часовой формат с битом PM — переводим в 24-часовой */
        hour = (uint8_t)(((hour & 0x7F) + 12) % 24);
    }

    console_print("20");
    print_2digit(year);
    console_print("-");
    print_2digit(month);
    console_print("-");
    print_2digit(day);
    console_print(" ");
    print_2digit(hour);
    console_print(":");
    print_2digit(minute);
    console_print(":");
    print_2digit(second);
    console_print(" (CMOS RTC, часовой пояс как настроен в BIOS)\n");
}
