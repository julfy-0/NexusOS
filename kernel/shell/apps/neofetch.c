/* NexusOS: первое настоящее "приложение" ядра — neofetch-like системная
 * сводка. Пока живёт прямо в kernel-space (userspace ещё не существует),
 * но написана как самостоятельный модуль, чтобы её было легко вынести
 * позже, когда появятся процессы. */
#include "neofetch.h"
#include "console.h"
#include "cpu.h"
#include "pit.h"
#include "kstate.h"
#include "boot_info.h"
#include "nexus_version.h"

#define LOGO_WIDTH 20

static const char *logo[] = {
    "     _   _         ",
    "    | \\ | | _____  ",
    "    |  \\| |/ _ \\ \\ ",
    "    | |\\  |  __/> >",
    "    |_| \\_|\\___/_/ ",
    "                    ",
    "       NexusOS      ",
    "                    ",
};
#define LOGO_LINES (int)(sizeof(logo) / sizeof(logo[0]))

/* Строк с информацией теперь больше, чем строк ASCII-лого (11 против 8) —
 * для строк за пределами logo[] печатаем просто отступ нужной ширины. */
static const char *logo_line(int row) {
    if (row < LOGO_LINES) return logo[row];
    return "";
}

static void print_padded(const char *s, int width, uint32_t color) {
    console_set_color(color, COLOR_BLACK);
    int n = 0;
    while (s[n]) {
        console_putchar(s[n]);
        n++;
    }
    for (; n < width; n++) console_putchar(' ');
    console_set_color(COLOR_WHITE, COLOR_BLACK);
}

void neofetch_run(void) {
    nexus_boot_info_t *bi = kstate_get_boot_info();

    char vendor[13];
    cpu_get_vendor(vendor);

    char brand[49];
    int have_brand = cpu_has_brand_string();
    if (have_brand) cpu_get_brand(brand);

    uint32_t cores = cpu_logical_cores();

    /* Реальное измерение через калибровку TSC по PIT — см. cpu.c.
     * Блокирует на ~150 мс, это ожидаемо (не баг, если neofetch на
     * секунду "задумывается" перед выводом). */
    uint32_t cpu_mhz = cpu_measure_freq_mhz();

    /* kstate_mem_summary() пересчитывает эти числа заново на каждый вызов
     * (не кэш), но источник — статичный снимок EFI memory map с момента
     * бута. Это НЕ живой трекинг реальных аллокаций — kmalloc/kfree ещё
     * не существует (см. docs/STATUS.md), так что "free" здесь означает
     * "было свободно на момент передачи управления ядру", а не "свободно
     * прямо сейчас". Как только появится heap — это надо будет заменить
     * на честную живую статистику аллокатора. */
    uint64_t total_pages = 0, conventional_pages = 0;
    kstate_mem_summary(&total_pages, &conventional_pages);
    uint64_t total_mb = (total_pages * 4096ULL) / (1024ULL * 1024ULL);
    uint64_t free_mb = (conventional_pages * 4096ULL) / (1024ULL * 1024ULL);

    uint64_t uptime_s = pit_get_uptime_seconds();
    uint64_t up_h = uptime_s / 3600;
    uint64_t up_m = (uptime_s % 3600) / 60;
    uint64_t up_sec = uptime_s % 60;

    console_print("\n");
    int row = 0;

    print_padded(logo_line(row++), LOGO_WIDTH, COLOR_GREEN);
    console_print("OS: NexusOS x86_64\n");

    print_padded(logo_line(row++), LOGO_WIDTH, COLOR_GREEN);
    console_print("Kernel: " NEXUS_VERSION_STRING "\n");

    print_padded(logo_line(row++), LOGO_WIDTH, COLOR_GREEN);
    console_print("User: root@nexusos\n");

    print_padded(logo_line(row++), LOGO_WIDTH, COLOR_GREEN);
    console_print("CPU Vendor: ");
    console_print(vendor);
    console_print("\n");

    print_padded(logo_line(row++), LOGO_WIDTH, COLOR_GREEN);
    if (have_brand) {
        console_print("CPU: ");
        console_print(brand);
    } else {
        console_print("CPU: (no brand string reported)");
    }
    console_print("\n");

    print_padded(logo_line(row++), LOGO_WIDTH, COLOR_GREEN);
    console_print("CPU Speed: ");
    if (cpu_mhz > 0) {
        console_print("~");
        console_print_dec(cpu_mhz);
        console_print(" MHz (measured via TSC/PIT)");
    } else {
        console_print("unknown");
    }
    console_print("\n");

    print_padded(logo_line(row++), LOGO_WIDTH, COLOR_GREEN);
    console_print("Cores (logical): ");
    console_print_dec(cores);
    console_print("\n");

    print_padded(logo_line(row++), LOGO_WIDTH, COLOR_GREEN);
    console_print("Memory: ");
    console_print_dec(free_mb);
    console_print(" MB free (at boot) / ");
    console_print_dec(total_mb);
    console_print(" MB detected\n");

    print_padded(logo_line(row++), LOGO_WIDTH, COLOR_GREEN);
    console_print("Uptime: ");
    console_print_dec(up_h);
    console_print("h ");
    console_print_dec(up_m);
    console_print("m ");
    console_print_dec(up_sec);
    console_print("s\n");

    print_padded(logo_line(row++), LOGO_WIDTH, COLOR_GREEN);
    console_print("Resolution: ");
    console_print_dec(bi->fb.width);
    console_print("x");
    console_print_dec(bi->fb.height);
    console_print("\n");

    print_padded(logo_line(row++), LOGO_WIDTH, COLOR_GREEN);
    console_print("Bootloader: NexusOS custom UEFI loader\n");

    while (row < LOGO_LINES) {
        print_padded(logo_line(row++), LOGO_WIDTH, COLOR_GREEN);
        console_print("\n");
    }

    console_print("\n");
}
