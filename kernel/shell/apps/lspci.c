#include <stdint.h>
#include "lspci.h"
#include "console.h"
#include "pci.h"

static void print_hex8(uint8_t v) {
    const char *digits = "0123456789ABCDEF";
    char buf[3] = { digits[(v >> 4) & 0xF], digits[v & 0xF], 0 };
    console_print(buf);
}

static void print_hex16(uint16_t v) {
    print_hex8((uint8_t)(v >> 8));
    print_hex8((uint8_t)(v & 0xFF));
}

void lspci_run(void) {
    int count = pci_get_device_count();
    if (count == 0) {
        console_print("lspci: no devices found (run after PCI scan at boot)\n");
        return;
    }

    for (int i = 0; i < count; i++) {
        const nexus_pci_device_t *d = pci_get_device(i);

        console_print_dec(d->bus);
        console_print(":");
        console_print_dec(d->device);
        console_print(".");
        console_print_dec(d->function);
        console_print("  ");

        console_set_color(COLOR_CYAN, COLOR_BLACK);
        print_hex16(d->vendor_id);
        console_print(":");
        print_hex16(d->device_id);
        console_set_color(COLOR_WHITE, COLOR_BLACK);

        console_print("  class ");
        print_hex8(d->class_code);
        console_print(" subclass ");
        print_hex8(d->subclass);
        console_print(" prog-if ");
        print_hex8(d->prog_if);
        console_print("\n");
    }
}
