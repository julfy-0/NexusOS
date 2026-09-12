/* NexusOS — x86_64 Page-Fault Diagnostics (0.5.2.4)
 *
 * Exception vector 14 is deliberately kept separate from the generic
 * exception path. A page fault gives us two pieces of information that are
 * essential when debugging the PMM/VMM/heap stack:
 *   - CR2: the virtual address that could not be accessed;
 *   - the CPU-provided error code: access type and protection state.
 *
 * This module is diagnostic only. It never tries to allocate, map, recover,
 * or resume from a fault. Recovery belongs to a future process/address-space
 * implementation.
 */
#include <stdint.h>
#include "page_fault.h"
#include "idt.h"
#include "paging.h"
#include "console.h"
#include "panic.h"

#define PF_ERR_PRESENT       (1ULL << 0)
#define PF_ERR_WRITE         (1ULL << 1)
#define PF_ERR_USER          (1ULL << 2)
#define PF_ERR_RESERVED      (1ULL << 3)
#define PF_ERR_INSTR_FETCH   (1ULL << 4)
#define PF_ERR_PROTECTION_KEY (1ULL << 5)
#define PF_ERR_SHADOW_STACK  (1ULL << 6)
#define PF_ERR_SGX           (1ULL << 15)

static uint64_t read_cr2(void) {
    uint64_t value;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

static void print_yes_no(const char *label, int value) {
    console_print(label);
    console_print(value ? "YES" : "NO");
}

static void print_access(uint64_t error) {
    console_print("  Access       : ");
    if (error & PF_ERR_WRITE) {
        console_print("WRITE");
    } else {
        console_print("READ");
    }

    if (error & PF_ERR_INSTR_FETCH) {
        console_print(" + INSTRUCTION FETCH");
    }
    console_print("\n");
}

static void print_reason(uint64_t error) {
    console_print("  Reason       : ");

    if (error & PF_ERR_SGX) {
        console_print("SGX violation");
    } else if (error & PF_ERR_SHADOW_STACK) {
        console_print("shadow-stack access violation");
    } else if (error & PF_ERR_PROTECTION_KEY) {
        console_print("protection-key violation");
    } else if (error & PF_ERR_RESERVED) {
        console_print("reserved bit set in paging structure");
    } else if (error & PF_ERR_PRESENT) {
        console_print("page protection violation");
    } else {
        console_print("page not present");
    }
    console_print("\n");
}

static void print_page_table_context(uint64_t fault_address) {
    /* Do not call vmm_is_mapped()/vmm_virt_to_phys() here. If the page-table
     * hierarchy itself is damaged, walking it from a page-fault handler can
     * recursively fault and destroy the diagnostic output. CR3 is safe and
     * gives us the root paging context without another memory walk. */
    console_print("  CR3          : ");
    console_print_hex(paging_get_cr3());
    console_print("\n  Page size     : 4096 bytes\n");

    console_print("  Fault address : ");
    console_print_hex(fault_address);
    console_print("\n");
}

void page_fault_handle(const interrupt_frame_t *frame) {
    uint64_t error = frame->err_code;
    uint64_t cr2 = read_cr2();
    int from_user = (error & PF_ERR_USER) != 0 || ((frame->cs & 0x3ULL) == 3);

    console_set_color(COLOR_WHITE, 0x1D1035);
    console_print("\n\n  *** NexusOS PAGE FAULT ***\n\n");

    console_print("  Vector       : 14 (#PF)\n");
    console_print("  Error code   : ");
    console_print_hex(error);
    console_print("\n");

    console_print("  CR2          : ");
    console_print_hex(cr2);
    console_print("\n");

    print_reason(error);
    print_access(error);

    console_print("  Mode         : ");
    console_print(from_user ? "USER (ring 3)" : "KERNEL (ring 0)");
    console_print("\n");

    print_yes_no("  Present      : ", (error & PF_ERR_PRESENT) != 0);
    console_print("\n");
    print_yes_no("  Write        : ", (error & PF_ERR_WRITE) != 0);
    console_print("\n");
    print_yes_no("  User         : ", (error & PF_ERR_USER) != 0);
    console_print("\n");
    print_yes_no("  Reserved     : ", (error & PF_ERR_RESERVED) != 0);
    console_print("\n");
    print_yes_no("  NX/fetch     : ", (error & PF_ERR_INSTR_FETCH) != 0);
    console_print("\n");

    if (error & PF_ERR_PROTECTION_KEY) {
        console_print("  Protection key: SET\n");
    }
    if (error & PF_ERR_SHADOW_STACK) {
        console_print("  Shadow stack : SET\n");
    }
    if (error & PF_ERR_SGX) {
        console_print("  SGX          : SET\n");
    }

    console_print("\n  RIP          : ");
    console_print_hex(frame->rip);
    console_print("\n  CS           : ");
    console_print_hex(frame->cs);
    console_print("\n  RFLAGS       : ");
    console_print_hex(frame->rflags);

    if ((frame->cs & 0x3ULL) == 3) {
        console_print("\n  User RSP     : ");
        console_print_hex(frame->user_rsp);
        console_print("\n  User SS      : ");
        console_print_hex(frame->ss);
    }

    console_print("\n\n");
    print_page_table_context(cr2);
    console_print("\n  Page-fault recovery is not implemented.\n");
    console_print("  The kernel will enter diagnostic restart mode.\n");

    panic_countdown_and_reboot();
}
