/* NexusOS: первое настоящее "приложение" ядра — neofetch-like системная
 * сводка. Пока живёт прямо в kernel-space (userspace ещё не существует),
 * но написана как самостоятельный модуль, чтобы её было легко вынести
 * позже, когда появятся процессы. */
#include "neofetch.h"
#include "console.h"
#include "cpu.h"
#include "kstate.h"
#include "boot_info.h"
#include "nexus_version.h"

#define LOGO_WIDTH 12
/* Байт-коды новых глифов рамки (см. font8x16.h) — используем вместо
 * UTF-8-литералов, т.к. консоль читает один байт = одна клетка, без
 * декодера многобайтовых последовательностей. */
#define B  "\x7F" /* █ */
#define TR "\x80" /* ╗ */
#define VB "\x81" /* ║ */
#define TL "\x82" /* ╔ */
#define BL "\x83" /* ╚ */
#define HB "\x84" /* ═ */
#define BR "\x85" /* ╝ */

static const char *logo[] = {
     " "B B B TR "   " B B TR,
     " "B B B B TR "  " B B VB,
     " "B B TL B B TR " " B B VB,
     " "B B VB BL B B TR B B VB,
     " "B B VB " " BL B B B B VB,
     " "BL HB BR "  " BL HB HB HB BR,
    "          ",
    "  NexusOS  ",
};

#define LOGO_LINES (int)(sizeof(logo) / sizeof(logo[0]))

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

/* Печатает "Label: " зелёным (в цвет логотипа), значение — уже обычным
 * белым цветом, которое выставляет print_padded для остатка строки. */
static void print_label(const char *label) {
    console_set_color(COLOR_GREEN, COLOR_BLACK);
    console_print(label);
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

    uint64_t total_pages = 0, conventional_pages = 0;
    kstate_mem_summary(&total_pages, &conventional_pages);
    uint64_t total_mb = (total_pages * 4096ULL) / (1024ULL * 1024ULL);
    uint64_t free_mb = (conventional_pages * 4096ULL) / (1024ULL * 1024ULL);

    console_print("\n");
    int row = 0;

    print_padded(logo[row++], LOGO_WIDTH, COLOR_GREEN);
    print_label("OS: ");
    console_print("NexusOS x86_64\n");

    print_padded(logo[row++], LOGO_WIDTH, COLOR_GREEN);
    print_label("Kernel: ");
    console_print(NEXUS_VERSION_STRING "\n");

    print_padded(logo[row++], LOGO_WIDTH, COLOR_GREEN);
    print_label("CPU Vendor: ");
    console_print(vendor);
    console_print("\n");

    print_padded(logo[row++], LOGO_WIDTH, COLOR_GREEN);
    print_label("CPU: ");
    if (have_brand) {
        console_print(brand);
    } else {
        console_print("(no brand string reported)");
    }
    console_print("\n");

    print_padded(logo[row++], LOGO_WIDTH, COLOR_GREEN);
    print_label("Cores (logical): ");
    console_print_dec(cores);
    console_print("\n");

    print_padded(logo[row++], LOGO_WIDTH, COLOR_GREEN);
    print_label("Memory: ");
    console_print_dec(free_mb);
    console_print(" MB free / ");
    console_print_dec(total_mb);
    console_print(" MB detected\n");

    print_padded(logo[row++], LOGO_WIDTH, COLOR_GREEN);
    print_label("Resolution: ");
    console_print_dec(bi->fb.width);
    console_print("x");
    console_print_dec(bi->fb.height);
    console_print("\n");

    print_padded(logo[row++], LOGO_WIDTH, COLOR_GREEN);
    print_label("Bootloader: ");
    console_print("NexusOS custom UEFI loader\n");

    while (row < LOGO_LINES) {
        print_padded(logo[row++], LOGO_WIDTH, COLOR_GREEN);
        console_print("\n");
    }

    console_print("\n");
}
