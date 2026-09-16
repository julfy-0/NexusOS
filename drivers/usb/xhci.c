#include <stdint.h>
#include <stddef.h>
#include "xhci.h"
#include "pci.h"
#include "paging.h"
#include "shell.h"
#include "gui.h"
#include "console.h"
#include "mouse.h"
#include "input.h"

#define XHCI_CLASS      0x0C
#define XHCI_SUBCLASS   0x03
#define XHCI_PROGIF     0x30

#define XHCI_USBCMD_RS    (1u << 0)
#define XHCI_USBCMD_HCRST (1u << 1)
#define XHCI_USBCMD_INTE  (1u << 2)
#define XHCI_USBSTS_HCH   (1u << 0)
#define XHCI_USBSTS_CNR   (1u << 11)

#define XHCI_PORTSC_CCS  (1u << 0)
#define XHCI_PORTSC_PED  (1u << 1)
#define XHCI_PORTSC_PR   (1u << 4)
#define XHCI_PORTSC_PP   (1u << 9)
#define XHCI_PORTSC_PRC  (1u << 21) /* в group "change" битов, RW1C */
#define XHCI_PORTSC_SPEED_SHIFT 10
#define XHCI_PORTSC_SPEED_MASK  0xFu
/* RW1C биты PORTSC (change-биты +事件-биты вида "запись 1 сбрасывает") —
 * их нельзя терять при read-modify-write, иначе случайно сбросим то, что
 * не собирались. */
#define XHCI_PORTSC_RW1C_MASK 0x00FE0002u /* CSC,PEC,WRC,OCC,PRC,PLC,CEC + PED(бит1 тоже RW1C-подобный: пишем 0, не 1, при r-m-w) */

#define SPIN_LIMIT 20000000u

/* ---------------------------------------------------------------------
 * TRB — базовый 16-байтовый блок и command/event/transfer ring.
 * ------------------------------------------------------------------- */
typedef struct {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} __attribute__((packed, aligned(16))) xhci_trb_t;

#define TRB_TYPE(ctrl)      (((ctrl) >> 10) & 0x3Fu)
#define TRB_CYCLE           0x1u
#define TRB_SET_TYPE(t)     (((uint32_t)(t) & 0x3Fu) << 10)

#define TRB_TYPE_NORMAL             1
#define TRB_TYPE_SETUP_STAGE        2
#define TRB_TYPE_DATA_STAGE         3
#define TRB_TYPE_STATUS_STAGE       4
#define TRB_TYPE_LINK               6
#define TRB_TYPE_ENABLE_SLOT_CMD    9
#define TRB_TYPE_ADDRESS_DEVICE_CMD 11
#define TRB_TYPE_CONFIGURE_EP_CMD   12
#define TRB_TYPE_EVALUATE_CTX_CMD   13
#define TRB_TYPE_TRANSFER_EVENT     32
#define TRB_TYPE_CMD_COMPLETION_EV  33
#define TRB_TYPE_PORT_STATUS_EV     34

#define TRB_CTRL_ENT  (1u << 1)
#define TRB_CTRL_ISP  (1u << 2)
#define TRB_CTRL_CH   (1u << 4)
#define TRB_CTRL_IOC  (1u << 5)
#define TRB_CTRL_IDT  (1u << 6)
#define TRB_CTRL_DIR_IN (1u << 16) /* Data/Status stage TRB: направление */

#define CMD_RING_TRBS   16
#define EVT_RING_TRBS   64
#define EP0_RING_TRBS   16
#define KBD_RING_TRBS   8
#define MAX_SCRATCHPAD  32
#define CTRL_BUF_SIZE   256

static volatile uint8_t *g_mmio;
static uint8_t g_ports;
static uint8_t g_connected;
static int g_ready;
static int g_kbd_present;
static int g_mouse_present;
static uint16_t g_vendor_id;
static uint16_t g_device_id;
static uint8_t g_pci_bus;
static uint8_t g_pci_device;
static uint8_t g_pci_function;
static uint64_t g_bar0;
static int g_xhci_controller_count;
static const char *g_last_error = "xHCI controller not initialized";
static uint8_t g_hid_protocol; /* 1 keyboard, 2 mouse */
static uint8_t g_hid_report_len;

static uint32_t g_op_base;      /* смещение operational-регистров от g_mmio */
static uint32_t g_db_base;      /* смещение doorbell array */
static uint32_t g_rt_base;      /* смещение runtime-регистров */
static uint32_t g_ctx_size;     /* 32 или 64 байта на контекст (HCCPARAMS1.CSZ) */
static uint32_t g_page_size;    /* PAGESIZE-регистр контроллера, обычно 4096 */

/* --- статическая (без malloc) DMA-память для структур контроллера ---
 * Всё это лежит в BSS ядра, то есть в первых BASE_IDENTITY_GIB (см.
 * kernel/mm/paging.c) — уже identity-mapped, дополнительно мапить не нужно. */
static xhci_trb_t g_cmd_ring[CMD_RING_TRBS]      __attribute__((aligned(64)));
static xhci_trb_t g_evt_ring[EVT_RING_TRBS]      __attribute__((aligned(64)));
static xhci_trb_t g_ep0_ring[EP0_RING_TRBS]      __attribute__((aligned(64)));
static xhci_trb_t g_kbd_ring[KBD_RING_TRBS]      __attribute__((aligned(64)));

typedef struct { uint64_t rsvdz0; uint32_t rsvdz1; uint32_t rsvdz2; } __attribute__((packed)) erst_entry_raw_t;
typedef struct {
    uint64_t ring_segment_base;
    uint32_t ring_segment_size; /* только младшие 16 бит значимы */
    uint32_t rsvdz;
} __attribute__((packed, aligned(16))) xhci_erst_entry_t;

static xhci_erst_entry_t g_erst[1] __attribute__((aligned(64)));

static uint64_t g_dcbaa[256] __attribute__((aligned(64))); /* индекс 0 = scratchpad array ptr */
static uint64_t g_scratchpad_array[MAX_SCRATCHPAD] __attribute__((aligned(64)));
static uint8_t  g_scratchpad_pages[MAX_SCRATCHPAD][4096] __attribute__((aligned(4096)));

/* Один слот — этого достаточно, чтобы поднять первую найденную HID
 * boot keyboard. Расширение на несколько устройств — отдельная задача. */
static uint8_t g_input_ctx[33 * 64] __attribute__((aligned(64)));
static uint8_t g_device_ctx[32 * 64] __attribute__((aligned(64)));

static uint8_t g_ctrl_buf[CTRL_BUF_SIZE] __attribute__((aligned(16)));
static uint8_t g_kbd_reports[KBD_RING_TRBS][8] __attribute__((aligned(16)));

static uint32_t g_cmd_enq = 0, g_cmd_cycle = 1;
static uint32_t g_evt_deq = 0, g_evt_cycle = 1;
static uint32_t g_ep0_enq = 0, g_ep0_cycle = 1;
static uint32_t g_kbd_enq = 0, g_kbd_cycle = 1;

static uint8_t g_slot_id = 0;
static uint8_t g_kbd_ep_dci = 0;   /* Device Context Index интерфейс-эндпоинта клавиатуры */
static uint8_t g_kbd_iface = 0;
static uint8_t g_last_report[8];

/* ---------------------------------------------------------------------
 * MMIO helpers
 * ------------------------------------------------------------------- */
static inline uint32_t mmio_read32(uint32_t off) {
    return *(volatile uint32_t *)(g_mmio + off);
}
static inline void mmio_write32(uint32_t off, uint32_t value) {
    *(volatile uint32_t *)(g_mmio + off) = value;
}
static inline uint64_t mmio_read64(uint32_t off) {
    return *(volatile uint64_t *)(g_mmio + off);
}
static inline void mmio_write64(uint32_t off, uint64_t value) {
    *(volatile uint64_t *)(g_mmio + off) = value;
}

static int wait_bit32(uint32_t off, uint32_t mask, int set) {
    for (uint32_t i = 0; i < SPIN_LIMIT; ++i) {
        uint32_t value = mmio_read32(off);
        if (!!(value & mask) == !!set) return 1;
    }
    return 0;
}

static void ring_doorbell(uint8_t slot_or_zero, uint32_t target) {
    mmio_write32(g_db_base + (uint32_t)slot_or_zero * 4u, target);
}

/* ---------------------------------------------------------------------
 * Device/Input Context — доступ по индексу с учётом переменного
 * (32 или 64 байта) размера контекста.
 * ------------------------------------------------------------------- */
static inline uint32_t *dctx_entry(uint8_t *base, int index) {
    return (uint32_t *)(base + (uint32_t)index * g_ctx_size);
}

/* Input Context: index 0 = Input Control Context, 1 = Slot, 2+ = EP по DCI-1. */
static inline uint32_t *input_ctrl_ctx(void)         { return dctx_entry(g_input_ctx, 0); }
static inline uint32_t *input_slot_ctx(void)          { return dctx_entry(g_input_ctx, 1); }
static inline uint32_t *input_ep_ctx(uint8_t dci)     { return dctx_entry(g_input_ctx, dci + 1); }
/* Device Context: index 0 = Slot, index (dci-1) = EP по DCI. */
static inline uint32_t *device_slot_ctx(void)         { return dctx_entry(g_device_ctx, 0); }

/* ---------------------------------------------------------------------
 * Command ring
 * ------------------------------------------------------------------- */
static void cmd_ring_push(uint64_t parameter, uint32_t status, uint32_t control) {
    xhci_trb_t *trb = &g_cmd_ring[g_cmd_enq];
    trb->parameter = parameter;
    trb->status = status;
    trb->control = (control & ~TRB_CYCLE) | (g_cmd_cycle ? TRB_CYCLE : 0);

    g_cmd_enq++;
    if (g_cmd_enq == CMD_RING_TRBS - 1) {
        /* Последний слот ring'а — Link TRB, всегда назад на 0. */
        xhci_trb_t *link = &g_cmd_ring[CMD_RING_TRBS - 1];
        link->parameter = (uint64_t)(uintptr_t)&g_cmd_ring[0];
        link->status = 0;
        link->control = TRB_SET_TYPE(TRB_TYPE_LINK) | (1u << 1) /* TC */
                       | (g_cmd_cycle ? TRB_CYCLE : 0);
        g_cmd_enq = 0;
        g_cmd_cycle ^= 1;
    }
}

/* Ждём Command Completion Event для команды, которую только что положили
 * (используем адрес TRB как ключ). Возвращает completion code (1=success)
 * или 0 при таймауте/событие не пришло. Дополнительно возвращает slot id
 * события через *out_slot, если он нужен вызывающему (Enable Slot). */
static uint32_t wait_command_completion(xhci_trb_t *cmd_trb, uint8_t *out_slot) {
    for (uint32_t spins = 0; spins < SPIN_LIMIT; spins++) {
        xhci_trb_t *ev = &g_evt_ring[g_evt_deq];
        uint32_t ctrl = ev->control;
        if (!!(ctrl & TRB_CYCLE) != !!g_evt_cycle) {
            continue; /* событий пока нет */
        }

        uint32_t type = TRB_TYPE(ctrl);
        uint64_t evt_deq_addr = (uint64_t)(uintptr_t)&g_evt_ring[g_evt_deq];

        if (type == TRB_TYPE_CMD_COMPLETION_EV) {
            uint32_t completion_code = (ev->status >> 24) & 0xFFu;
            uint64_t src = ev->parameter;
            uint8_t slot = (uint8_t)((ctrl >> 24) & 0xFFu);

            g_evt_deq++;
            if (g_evt_deq == EVT_RING_TRBS) { g_evt_deq = 0; g_evt_cycle ^= 1; }
            mmio_write64(g_rt_base + 0x20 + 0x18, evt_deq_addr | (1u << 3));

            if (src == (uint64_t)(uintptr_t)cmd_trb) {
                if (out_slot) *out_slot = slot;
                return completion_code;
            }
            /* Событие для другой команды (не должно происходить в нашей
             * строго последовательной инициализации) — пропускаем. */
            continue;
        }

        /* Что-то другое (Port Status Change и т.п.) во время ожидания
         * команды — просто продвигаем dequeue и идём дальше. */
        g_evt_deq++;
        if (g_evt_deq == EVT_RING_TRBS) { g_evt_deq = 0; g_evt_cycle ^= 1; }
        mmio_write64(g_rt_base + 0x20 + 0x18, evt_deq_addr | (1u << 3));
    }
    return 0;
}

static uint32_t run_command(uint64_t parameter, uint32_t status, uint32_t control, uint8_t *out_slot) {
    xhci_trb_t *slot_ptr = &g_cmd_ring[g_cmd_enq];
    cmd_ring_push(parameter, status, control);
    ring_doorbell(0, 0);
    return wait_command_completion(slot_ptr, out_slot);
}

/* ---------------------------------------------------------------------
 * Control transfers (EP0) — Setup/Data/Status TRB на transfer ring'e.
 * dir_in: 1 если Data stage IN (читаем из устройства), 0 если OUT/без данных.
 * ------------------------------------------------------------------- */
static xhci_trb_t *ep0_ring_push_raw(uint64_t parameter, uint32_t status, uint32_t control) {
    xhci_trb_t *trb = &g_ep0_ring[g_ep0_enq];
    trb->parameter = parameter;
    trb->status = status;
    trb->control = (control & ~TRB_CYCLE) | (g_ep0_cycle ? TRB_CYCLE : 0);

    g_ep0_enq++;
    if (g_ep0_enq == EP0_RING_TRBS - 1) {
        xhci_trb_t *link = &g_ep0_ring[EP0_RING_TRBS - 1];
        link->parameter = (uint64_t)(uintptr_t)&g_ep0_ring[0];
        link->status = 0;
        link->control = TRB_SET_TYPE(TRB_TYPE_LINK) | (1u << 1)
                       | (g_ep0_cycle ? TRB_CYCLE : 0);
        g_ep0_enq = 0;
        g_ep0_cycle ^= 1;
    }
    return trb;
}

static int wait_transfer_completion(xhci_trb_t *last_trb) {
    for (uint32_t spins = 0; spins < SPIN_LIMIT; spins++) {
        xhci_trb_t *ev = &g_evt_ring[g_evt_deq];
        uint32_t ctrl = ev->control;
        if (!!(ctrl & TRB_CYCLE) != !!g_evt_cycle) continue;

        uint32_t type = TRB_TYPE(ctrl);
        uint64_t evt_deq_addr = (uint64_t)(uintptr_t)&g_evt_ring[g_evt_deq];
        uint64_t src = ev->parameter;
        uint32_t completion_code = (ev->status >> 24) & 0xFFu;

        g_evt_deq++;
        if (g_evt_deq == EVT_RING_TRBS) { g_evt_deq = 0; g_evt_cycle ^= 1; }
        mmio_write64(g_rt_base + 0x20 + 0x18, evt_deq_addr | (1u << 3));

        if (type == TRB_TYPE_TRANSFER_EVENT && src == (uint64_t)(uintptr_t)last_trb) {
            return (completion_code == 1 || completion_code == 13) ? 1 : 0; /* 13 = Short Packet, ок */
        }
    }
    return 0;
}

/* bmRequestType/bRequest/wValue/wIndex/wLength — стандартный control
 * transfer. buf/len — буфер данных (может быть NULL/0 для no-data). */
static int control_transfer(uint8_t bmRequestType, uint8_t bRequest,
                             uint16_t wValue, uint16_t wIndex, uint16_t wLength,
                             void *buf, int dir_in) {
    uint64_t setup_packet;
    uint8_t sp[8];
    sp[0] = bmRequestType; sp[1] = bRequest;
    sp[2] = (uint8_t)(wValue & 0xFF); sp[3] = (uint8_t)(wValue >> 8);
    sp[4] = (uint8_t)(wIndex & 0xFF); sp[5] = (uint8_t)(wIndex >> 8);
    sp[6] = (uint8_t)(wLength & 0xFF); sp[7] = (uint8_t)(wLength >> 8);
    __builtin_memcpy(&setup_packet, sp, 8);

    uint32_t trt = (wLength == 0) ? 0 : (dir_in ? 3u : 2u);
    ep0_ring_push_raw(setup_packet, 8u,
        TRB_SET_TYPE(TRB_TYPE_SETUP_STAGE) | TRB_CTRL_IDT | (trt << 16));

    xhci_trb_t *last;
    if (wLength != 0) {
        last = ep0_ring_push_raw((uint64_t)(uintptr_t)buf, wLength,
            TRB_SET_TYPE(TRB_TYPE_DATA_STAGE) | (dir_in ? TRB_CTRL_DIR_IN : 0) | TRB_CTRL_IOC);
        ring_doorbell(g_slot_id, 1);
        if (!wait_transfer_completion(last)) return 0;

        last = ep0_ring_push_raw(0, 0,
            TRB_SET_TYPE(TRB_TYPE_STATUS_STAGE) | (dir_in ? 0 : TRB_CTRL_DIR_IN) | TRB_CTRL_IOC);
        ring_doorbell(g_slot_id, 1);
        return wait_transfer_completion(last);
    }

    last = ep0_ring_push_raw(0, 0,
        TRB_SET_TYPE(TRB_TYPE_STATUS_STAGE) | TRB_CTRL_DIR_IN | TRB_CTRL_IOC);
    ring_doorbell(g_slot_id, 1);
    return wait_transfer_completion(last);
}

/* ---------------------------------------------------------------------
 * xHCI Legacy Support handoff (Extended Capabilities, capability id 1).
 * ------------------------------------------------------------------- */
static int xhci_legacy_handoff(uint32_t hccparams1) {
    uint32_t xecp = (hccparams1 >> 16) & 0xFFFFu;
    if (xecp == 0) return 1;

    uint32_t off = xecp * 4u;
    for (int guard = 0; guard < 256 && off != 0; guard++) {
        uint32_t cap = mmio_read32(off);
        uint32_t cap_id = cap & 0xFFu;
        uint32_t next = (cap >> 8) & 0xFFu;

        if (cap_id == 1) { /* USB Legacy Support */
            uint32_t v = mmio_read32(off);
            const uint32_t BIOS_OWNED = (1u << 16);
            const uint32_t OS_OWNED   = (1u << 24);

            /* Claim the controller only after the BIOS-owned semaphore is
             * observed. Some firmware leaves SMI generation enabled; clear
             * it before handing the controller to the OS. */
            mmio_write32(off, (v & ~0xFFFFu) | OS_OWNED);
            for (uint32_t i = 0; i < 2000000u; i++) {
                v = mmio_read32(off);
                if (!(v & BIOS_OWNED)) return (v & OS_OWNED) ? 1 : 0;
            }
            return 0;
        }

        if (next == 0) break;
        off += next * 4u;
    }
    return 1;
}

/* ---------------------------------------------------------------------
 * HID boot keyboard: usage id (0x04..) -> ASCII, US QWERTY.
 * ------------------------------------------------------------------- */
static const char hid_ascii[104] = {
    /* 0x00-0x03 */ 0,0,0,0,
    /* 0x04-0x1D a-z */ 'a','b','c','d','e','f','g','h','i','j','k','l','m',
                        'n','o','p','q','r','s','t','u','v','w','x','y','z',
    /* 0x1E-0x27 1-9,0 */ '1','2','3','4','5','6','7','8','9','0',
    /* 0x28 Enter */ '\n',
    /* 0x29 Esc */ 27,
    /* 0x2A Backspace */ '\b',
    /* 0x2B Tab */ '\t',
    /* 0x2C Space */ ' ',
    /* 0x2D - */ '-', /* 0x2E = */ '=', /* 0x2F [ */ '[', /* 0x30 ] */ ']',
    /* 0x31 \ */ '\\', /* 0x32 (Europe1) */ 0, /* 0x33 ; */ ';', /* 0x34 ' */ '\'',
    /* 0x35 ` */ '`', /* 0x36 , */ ',', /* 0x37 . */ '.', /* 0x38 / */ '/',
    /* 0x39 CapsLock */ 0,
    /* 0x3A-0x45 F1-F12 */ 0,0,0,0,0,0,0,0,0,0,0,0,
    /* 0x46-0x49 PrintScr/ScrollLock/Pause/Insert */ 0,0,0,0,
    /* 0x4A Home */ 0,
    /* 0x4B PageUp */ 0x7F,
    /* 0x4C Delete */ 0,
    /* 0x4D End */ 0,
    /* 0x4E PageDown */ 0x7E,
    /* 0x4F RightArrow */ (char)0x82,
    /* 0x50 LeftArrow */ (char)0x81,
    /* 0x51 DownArrow */ (char)0x84,
    /* 0x52 UpArrow */ (char)0x83,
};

static const char hid_ascii_shift[104] = {
    0,0,0,0,
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '!','@','#','$','%','^','&','*','(',')',
    '\n', 27, '\b', '\t', ' ',
    '_','+','{','}','|',0,':','"','~','<','>','?',
    0,
    0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,
    0, 0x7F, 0, 0, 0x7E,
    (char)0x82, (char)0x81, (char)0x84, (char)0x83,
};

/* Реальные "спец" коды (PageUp/PageDown/стрелки) обрабатываются отдельно
 * (как extended-скан-коды у PS/2), а не через таблицу ASCII напрямую —
 * см. dispatch_hid_key(). Таблицы выше используют их только как маркеры. */
#define HID_PAGE_UP    0x4B
#define HID_PAGE_DOWN  0x4E
#define HID_ARROW_RIGHT 0x4F
#define HID_ARROW_LEFT  0x50
#define HID_ARROW_DOWN  0x51
#define HID_ARROW_UP    0x52

static void dispatch_hid_key(uint8_t usage, int shift, int ctrl) {
    if (!gui_is_active() && ctrl && usage == HID_ARROW_UP) {
        console_scroll(1);
        return;
    }
    if (!gui_is_active() && ctrl && usage == HID_ARROW_DOWN) {
        console_scroll(-1);
        return;
    }

    if (usage == HID_PAGE_UP) { console_scroll(1); return; }
    if (usage == HID_PAGE_DOWN) { console_scroll(-1); return; }

    if (usage == HID_ARROW_UP || usage == HID_ARROW_DOWN ||
        usage == HID_ARROW_LEFT || usage == HID_ARROW_RIGHT) {
        if (gui_is_active()) {
            int key = (usage == HID_ARROW_UP) ? GUI_KEY_UP :
                      (usage == HID_ARROW_DOWN) ? GUI_KEY_DOWN :
                      (usage == HID_ARROW_LEFT) ? GUI_KEY_LEFT : GUI_KEY_RIGHT;
            (void)gui_handle_key(key);
        } else {
            if (ctrl) {
                /* Ctrl+Arrow was consumed above for scrollback. */
                return;
            }
            if (usage == HID_ARROW_UP) shell_history_prev();
            else if (usage == HID_ARROW_DOWN) shell_history_next();
        }
        return;
    }

    if (usage >= 104) return;
    char c = shift ? hid_ascii_shift[usage] : hid_ascii[usage];
    if (c == 0) return;

    int was_gui = gui_is_active();
    if (was_gui) {
        (void)gui_handle_key((int)(unsigned char)c);
        if (was_gui && !gui_is_active()) shell_return_from_desktop();
    } else {
        shell_input_char(c);
    }
}

static void process_boot_report(const uint8_t *report) {
    uint8_t modifiers = report[0];
    int shift = (modifiers & 0x22) != 0; /* bit1 LShift, bit5 RShift */
    int ctrl = (modifiers & 0x11) != 0;  /* bit0 LCtrl, bit4 RCtrl */

    for (int i = 2; i < 8; i++) {
        uint8_t usage = report[i];
        if (usage == 0 || usage == 1) continue; /* 0=пусто, 1=rollover error */

        int was_down = 0;
        for (int j = 2; j < 8; j++) if (g_last_report[j] == usage) { was_down = 1; break; }
        if (!was_down) dispatch_hid_key(usage, shift, ctrl);
    }

    for (int i = 0; i < 8; i++) g_last_report[i] = report[i];
}

/* ---------------------------------------------------------------------
 * Interrupt (keyboard) ring
 * ------------------------------------------------------------------- */
static void kbd_ring_push_normal(int index) {
    xhci_trb_t *trb = &g_kbd_ring[g_kbd_enq];
    trb->parameter = (uint64_t)(uintptr_t)g_kbd_reports[index];
    trb->status = g_hid_report_len ? g_hid_report_len : 8u; /* HID boot report */
    trb->control = (trb->control & ~TRB_CYCLE);
    trb->control = TRB_SET_TYPE(TRB_TYPE_NORMAL) | TRB_CTRL_IOC | TRB_CTRL_ISP
                 | (g_kbd_cycle ? TRB_CYCLE : 0);

    g_kbd_enq++;
    if (g_kbd_enq == KBD_RING_TRBS - 1) {
        xhci_trb_t *link = &g_kbd_ring[KBD_RING_TRBS - 1];
        link->parameter = (uint64_t)(uintptr_t)&g_kbd_ring[0];
        link->status = 0;
        link->control = TRB_SET_TYPE(TRB_TYPE_LINK) | (1u << 1)
                       | (g_kbd_cycle ? TRB_CYCLE : 0);
        g_kbd_enq = 0;
        g_kbd_cycle ^= 1;
    }
}

/* ---------------------------------------------------------------------
 * Инициализация
 * ------------------------------------------------------------------- */
static int setup_scratchpad(uint32_t hcsparams2) {
    uint32_t max_scratch = (((hcsparams2 >> 21) & 0x1Fu) << 5) | ((hcsparams2 >> 27) & 0x1Fu);
    if (max_scratch == 0) {
        g_dcbaa[0] = 0;
        return 1;
    }
    if (max_scratch > MAX_SCRATCHPAD) return 0; /* больше не поддерживаем — не тот случай для наc */

    for (uint32_t i = 0; i < max_scratch; i++) {
        for (int b = 0; b < 4096; b++) g_scratchpad_pages[i][b] = 0;
        g_scratchpad_array[i] = (uint64_t)(uintptr_t)g_scratchpad_pages[i];
    }
    g_dcbaa[0] = (uint64_t)(uintptr_t)g_scratchpad_array;
    return 1;
}

static int find_hid_boot(const uint8_t *cfg, uint16_t total_len,
                              uint8_t *out_iface, uint8_t *out_ep_addr,
                              uint8_t *out_ep_maxpkt, uint8_t *out_ep_interval,
                              uint8_t *out_config_value, uint8_t *out_protocol) {
    *out_config_value = cfg[5]; /* bConfigurationValue в Configuration Descriptor */
    uint16_t off = 0;
    int in_hid_boot = 0;
    uint8_t cur_iface = 0;
    uint8_t cur_protocol = 0;

    while (off + 2 <= total_len) {
        uint8_t len = cfg[off];
        uint8_t type = cfg[off + 1];
        if (len == 0) break;

        if (type == 0x04 && off + 9 <= total_len) { /* Interface Descriptor */
            uint8_t iface_class = cfg[off + 5];
            uint8_t iface_subclass = cfg[off + 6];
            uint8_t iface_protocol = cfg[off + 7];
            cur_iface = cfg[off + 2];
            cur_protocol = iface_protocol;
            in_hid_boot = (iface_class == 0x03 && iface_subclass == 0x01 && (iface_protocol == 0x01 || iface_protocol == 0x02));

        } else if (type == 0x05 && off + 7 <= total_len && in_hid_boot) { /* Endpoint Descriptor */
            uint8_t ep_addr = cfg[off + 2];
            uint8_t ep_attr = cfg[off + 3];
            if ((ep_addr & 0x80) && (ep_attr & 0x03) == 0x03) { /* Interrupt IN */
                *out_iface = cur_iface;
                *out_ep_addr = ep_addr;
                *out_ep_maxpkt = cfg[off + 4];
                *out_ep_interval = cfg[off + 6];
                *out_protocol = cur_protocol;
                return 1;
            }
        }
        off = (uint16_t)(off + len);
    }
    return 0;
}

static uint32_t interval_to_xhci(uint8_t bInterval, int is_lowspeed_or_fullspeed) {
    /* Slot/EP Context Interval — в единицах 2^n * 125us.
     * Для LS/FS bInterval уже в миллисекундах (1-255) -> ближайшая степень
     * двойки *8 (125us*8=1ms). Для HS/SS bInterval это сам показатель степени. */
    if (!is_lowspeed_or_fullspeed) {
        uint32_t n = bInterval;
        if (n == 0) n = 1;
        return n - 1;
    }
    uint32_t ms = bInterval ? bInterval : 1;
    uint32_t n = 3; /* 2^3 * 125us = 1ms, минимум для LS/FS периодических EP */
    while ((1u << (n - 3)) < ms && n < 10) n++;
    return n;
}

static int setup_hid_on_port(uint8_t port_index /* 0-based */) {
    uint32_t port_base = g_op_base + 0x400 + (uint32_t)port_index * 0x10u;

    uint32_t portsc = mmio_read32(port_base);
    if (!(portsc & XHCI_PORTSC_CCS)) return 0;

    /* Port Reset. */
    uint32_t v = (portsc & ~XHCI_PORTSC_RW1C_MASK) | XHCI_PORTSC_PR;
    mmio_write32(port_base, v);
    if (!wait_bit32(port_base, XHCI_PORTSC_PRC, 1)) return 0;
    portsc = mmio_read32(port_base);
    mmio_write32(port_base, (portsc & ~(XHCI_PORTSC_RW1C_MASK & ~XHCI_PORTSC_PRC)) | XHCI_PORTSC_PRC);
    portsc = mmio_read32(port_base);
    if (!(portsc & XHCI_PORTSC_PED)) return 0;

    uint32_t speed = (portsc >> XHCI_PORTSC_SPEED_SHIFT) & XHCI_PORTSC_SPEED_MASK;
    if (speed == 0) speed = 1;
    int is_ls_fs = (speed == 1 || speed == 2);
    uint16_t ep0_maxpkt = (speed == 4) ? 512 : (speed == 3) ? 64 : (speed == 2) ? 8 : 8;

    /* Enable Slot */
    uint8_t slot = 0;
    uint32_t cc = run_command(0, 0, TRB_SET_TYPE(TRB_TYPE_ENABLE_SLOT_CMD), &slot);
    if (cc != 1 || slot == 0) return 0;
    g_slot_id = slot;

    for (unsigned i = 0; i < sizeof(g_device_ctx); i++) g_device_ctx[i] = 0;
    g_dcbaa[slot] = (uint64_t)(uintptr_t)g_device_ctx;

    for (unsigned i = 0; i < sizeof(g_input_ctx); i++) g_input_ctx[i] = 0;
    input_ctrl_ctx()[1] = (1u << 0) | (1u << 1); /* Add: A0 (slot) + A1 (EP0) */

    uint32_t *slot_ctx = input_slot_ctx();
    slot_ctx[0] = (1u << 27) /* Context Entries = 1 */ | (speed << 20);
    slot_ctx[1] = ((uint32_t)(port_index + 1) << 16); /* Root Hub Port Number */

    uint32_t *ep0_ctx = input_ep_ctx(1);
    ep0_ctx[1] = (3u << 1) /* CErr=3 */ | (4u << 3) /* EP Type = Control */
               | ((uint32_t)ep0_maxpkt << 16);
    ep0_ctx[2] = (uint32_t)(uintptr_t)&g_ep0_ring[0] | 1u /* DCS */;
    ep0_ctx[3] = 0;
    ep0_ctx[4] = (8u << 0); /* Average TRB Length */

    g_ep0_enq = 0; g_ep0_cycle = 1;
    for (int i = 0; i < EP0_RING_TRBS; i++) { g_ep0_ring[i].parameter = 0; g_ep0_ring[i].status = 0; g_ep0_ring[i].control = 0; }

    cc = run_command((uint64_t)(uintptr_t)g_input_ctx, 0,
                      TRB_SET_TYPE(TRB_TYPE_ADDRESS_DEVICE_CMD) | ((uint32_t)slot << 24), NULL);
    if (cc != 1) return 0;

    /* Читаем первые 8 байт Device Descriptor — там настоящий bMaxPacketSize0. */
    if (!control_transfer(0x80, 0x06, (1u << 8) | 0, 0, 8, g_ctrl_buf, 1)) return 0;
    uint8_t real_maxpkt = g_ctrl_buf[7];
    if (real_maxpkt != 0 && real_maxpkt != ep0_maxpkt) {
        input_ctrl_ctx()[0] = 0;
        input_ctrl_ctx()[1] = (1u << 1); /* Evaluate только EP0 */
        uint32_t *ep0_ctx2 = input_ep_ctx(1);
        ep0_ctx2[1] = (ep0_ctx2[1] & ~(0xFFFFu << 16)) | ((uint32_t)real_maxpkt << 16);
        run_command((uint64_t)(uintptr_t)g_input_ctx, 0,
                    TRB_SET_TYPE(TRB_TYPE_EVALUATE_CTX_CMD) | ((uint32_t)slot << 24), NULL);
    }

    /* Полный Device Descriptor (18 байт) — не обязателен для остального,
     * но полезен на будущее (vendor/product id и т.п.), читаем и игнорируем. */
    (void)control_transfer(0x80, 0x06, (1u << 8) | 0, 0, 18, g_ctrl_buf, 1);

    /* Configuration Descriptor: сперва 9 байт, чтобы узнать wTotalLength. */
    if (!control_transfer(0x80, 0x06, (2u << 8) | 0, 0, 9, g_ctrl_buf, 1)) return 0;
    uint16_t total_len = (uint16_t)(g_ctrl_buf[2] | ((uint16_t)g_ctrl_buf[3] << 8));
    if (total_len > CTRL_BUF_SIZE) total_len = CTRL_BUF_SIZE;
    if (!control_transfer(0x80, 0x06, (2u << 8) | 0, 0, total_len, g_ctrl_buf, 1)) return 0;

    uint8_t iface = 0, ep_addr = 0, ep_maxpkt = 0, ep_interval = 0, config_value = 0;
    uint8_t protocol = 0;
    if (!find_hid_boot(g_ctrl_buf, total_len, &iface, &ep_addr, &ep_maxpkt, &ep_interval, &config_value, &protocol)) {
        return 0; /* устройство не HID boot keyboard/mouse */
    }

    if (!control_transfer(0x00, 0x09, config_value, 0, 0, NULL, 0)) return 0; /* SET_CONFIGURATION */
    (void)control_transfer(0x21, 0x0B, 0, iface, 0, NULL, 0); /* SET_PROTOCOL(Boot) — best effort */
    (void)control_transfer(0x21, 0x0A, 0, iface, 0, NULL, 0); /* SET_IDLE(0) — best effort */

    uint8_t ep_num = ep_addr & 0x0Fu;
    uint8_t dci = (uint8_t)(ep_num * 2u + 1u); /* IN endpoint */
    g_kbd_ep_dci = dci;
    g_kbd_iface = iface;
    g_hid_protocol = protocol;
    g_hid_report_len = protocol == 0x02 ? 4 : 8;

    input_ctrl_ctx()[0] = 0;
    input_ctrl_ctx()[1] = (1u << 0) | (1u << dci); /* A0 (slot, для Context Entries) + Adci */
    slot_ctx = input_slot_ctx();
    slot_ctx[0] = (((uint32_t)(dci > 1 ? dci : 1)) << 27) | (speed << 20);
    slot_ctx[1] = ((uint32_t)(port_index + 1) << 16);

    g_kbd_enq = 0; g_kbd_cycle = 1;
    for (int i = 0; i < KBD_RING_TRBS; i++) { g_kbd_ring[i].parameter = 0; g_kbd_ring[i].status = 0; g_kbd_ring[i].control = 0; }

    uint32_t *kbd_ctx = input_ep_ctx(dci);
    uint32_t interval = interval_to_xhci(ep_interval, is_ls_fs);
    kbd_ctx[0] = interval << 16;
    kbd_ctx[1] = (3u << 1) /* CErr=3 */ | (7u << 3) /* EP Type = Interrupt IN */
               | ((uint32_t)ep_maxpkt << 16);
    kbd_ctx[2] = (uint32_t)(uintptr_t)&g_kbd_ring[0] | 1u;
    kbd_ctx[3] = 0;
    kbd_ctx[4] = (uint32_t)ep_maxpkt;

    cc = run_command((uint64_t)(uintptr_t)g_input_ctx, 0,
                      TRB_SET_TYPE(TRB_TYPE_CONFIGURE_EP_CMD) | ((uint32_t)slot << 24), NULL);
    if (cc != 1) return 0;

    for (int i = 0; i < 8; i++) g_last_report[i] = 0;
    for (int i = 0; i < KBD_RING_TRBS - 1; i++) kbd_ring_push_normal(i % KBD_RING_TRBS);
    ring_doorbell(slot, dci);

    return 1;
}

static int xhci_try_controller(const nexus_pci_device_t *dev) {
    g_ready = 0;
    g_kbd_present = 0;
    g_mouse_present = 0;
    g_hid_protocol = 0;
    g_hid_report_len = 0;
    g_mmio = 0;
    g_ports = 0;
    g_connected = 0;

    if (!dev) {
        g_last_error = "null xHCI controller descriptor";
        return 0;
    }

    g_vendor_id = dev->vendor_id;
    g_device_id = dev->device_id;
    g_pci_bus = dev->bus;
    g_pci_device = dev->device;
    g_pci_function = dev->function;

    uint64_t bar = pci_get_bar64(dev, 0);
    if (bar == 0) { g_last_error = "xHCI BAR0 is invalid"; return 0; }
    if (bar >= (512ULL * 1024ULL * 1024ULL * 1024ULL)) { g_last_error = "xHCI BAR0 is outside supported address space"; return 0; }
    g_bar0 = bar;

    pci_enable_device(dev, 1, 1);

    /* BAR может лежать вне всего, что уже замаплено paging_init()
     * (базовый диапазон, EFI memory map, framebuffer) — MMIO там не
     * числится. Мапим явно и некэшируемо (см. kernel/mm/paging.c), иначе
     * первое же чтение регистра — page fault, а doorbell-записи молча
     * зависали бы в кэше. */
    paging_map_region(bar, bar + 0x200000ULL);

    g_mmio = (volatile uint8_t *)(uintptr_t)bar;

    uint32_t cap0 = mmio_read32(0x00);
    uint32_t caplength = cap0 & 0xFFu;
    uint32_t version = (cap0 >> 16) & 0xFFFFu;
    if (caplength < 0x20u || caplength > 0xFFu || version == 0 || version == 0xFFFFu) { g_last_error = "invalid xHCI capability registers"; return 0; }

    uint32_t hcsparams1 = mmio_read32(0x04);
    uint32_t hcsparams2 = mmio_read32(0x08);
    uint32_t hccparams1 = mmio_read32(0x10);
    uint32_t dboff = mmio_read32(0x14) & ~0x3u;
    uint32_t rtsoff = mmio_read32(0x18) & ~0x1Fu;

    g_ports = (uint8_t)((hcsparams1 >> 24) & 0xFFu);
    if (g_ports > 127u) { g_last_error = "invalid xHCI root-port count"; return 0; }

    g_op_base = caplength;
    g_db_base = dboff;
    g_rt_base = rtsoff;
    g_ctx_size = (hccparams1 & (1u << 2)) ? 64u : 32u;

    if (!xhci_legacy_handoff(hccparams1)) { g_last_error = "xHCI BIOS ownership handoff timed out"; return 0; }

    /* Остановить контроллер перед reset. */
    uint32_t cmd = mmio_read32(g_op_base + 0x00);
    uint32_t sts = mmio_read32(g_op_base + 0x04);
    if (sts & XHCI_USBSTS_CNR) {
        g_last_error = "xHCI controller not ready";
        return 0;
    }
    cmd &= ~XHCI_USBCMD_RS;
    mmio_write32(g_op_base + 0x00, cmd);
    if (!wait_bit32(g_op_base + 0x04, XHCI_USBSTS_HCH, 1)) { g_last_error = "xHCI did not halt before reset"; return 0; }

    /* Host Controller Reset. */
    cmd = mmio_read32(g_op_base + 0x00);
    mmio_write32(g_op_base + 0x00, cmd | XHCI_USBCMD_HCRST);
    if (!wait_bit32(g_op_base + 0x00, XHCI_USBCMD_HCRST, 0)) { g_last_error = "xHCI controller reset timed out"; return 0; }
    if (!wait_bit32(g_op_base + 0x04, XHCI_USBSTS_CNR, 0)) { g_last_error = "xHCI controller not ready after reset"; return 0; }
    if (!wait_bit32(g_op_base + 0x04, XHCI_USBSTS_HCH, 1)) { g_last_error = "xHCI did not return to halted state after reset"; return 0; }

    g_page_size = (mmio_read32(g_op_base + 0x08) & 0xFFFFu) << 12;
    if (g_page_size == 0) g_page_size = 4096;

    /* CONFIG: сколько device slots реально включаем. */
    uint32_t max_slots = hcsparams1 & 0xFFu;
    if (max_slots == 0) max_slots = 1;
    if (max_slots > 255) max_slots = 255;
    mmio_write32(g_op_base + 0x38, max_slots);

    for (int i = 0; i < 256; i++) g_dcbaa[i] = 0;
    if (!setup_scratchpad(hcsparams2)) { g_last_error = "unsupported xHCI scratchpad requirement"; return 0; }
    mmio_write64(g_op_base + 0x30, (uint64_t)(uintptr_t)g_dcbaa);

    /* Command ring. */
    g_cmd_enq = 0; g_cmd_cycle = 1;
    for (int i = 0; i < CMD_RING_TRBS; i++) { g_cmd_ring[i].parameter = 0; g_cmd_ring[i].status = 0; g_cmd_ring[i].control = 0; }
    mmio_write64(g_op_base + 0x18, (uint64_t)(uintptr_t)&g_cmd_ring[0] | 1u /* RCS */);

    /* Event ring: один сегмент, ERST с одной записью. */
    g_evt_deq = 0; g_evt_cycle = 1;
    for (int i = 0; i < EVT_RING_TRBS; i++) { g_evt_ring[i].parameter = 0; g_evt_ring[i].status = 0; g_evt_ring[i].control = 0; }
    g_erst[0].ring_segment_base = (uint64_t)(uintptr_t)&g_evt_ring[0];
    g_erst[0].ring_segment_size = EVT_RING_TRBS;
    g_erst[0].rsvdz = 0;

    mmio_write32(g_rt_base + 0x20 + 0x08, 1); /* ERSTSZ = 1 запись */
    mmio_write64(g_rt_base + 0x20 + 0x18, (uint64_t)(uintptr_t)&g_evt_ring[0]); /* ERDP */
    mmio_write64(g_rt_base + 0x20 + 0x10, (uint64_t)(uintptr_t)&g_erst[0]);     /* ERSTBA — активирует ring */
    mmio_write32(g_rt_base + 0x20 + 0x00, 0); /* IMAN: interrupts выключены — работаем поллингом */

    /* Запуск контроллера. */
    cmd = mmio_read32(g_op_base + 0x00);
    cmd |= XHCI_USBCMD_RS;
    mmio_write32(g_op_base + 0x00, cmd);
    if (!wait_bit32(g_op_base + 0x04, XHCI_USBSTS_HCH, 0)) {
        g_last_error = "xHCI failed to start";
        return 0;
    }

    uint32_t port_base = g_op_base + 0x400;
    int first_connected = -1;
    for (uint8_t port = 0; port < g_ports; ++port) {
        uint32_t portsc = mmio_read32(port_base + (uint32_t)port * 0x10u);
        if (portsc & XHCI_PORTSC_CCS) {
            ++g_connected;
            if (first_connected < 0) first_connected = port;
        }
    }

    g_ready = 1;
    g_last_error = "none";

    if (first_connected >= 0) {
        int hid_ok = setup_hid_on_port((uint8_t)first_connected);
        if (hid_ok && g_hid_protocol == 0x01) {
            g_kbd_present = 1;
            input_set_present(NEXUS_INPUT_SOURCE_USB, NEXUS_INPUT_DEVICE_KEYBOARD, 1);
        } else if (hid_ok && g_hid_protocol == 0x02) {
            g_mouse_present = 1;
            input_set_present(NEXUS_INPUT_SOURCE_USB, NEXUS_INPUT_DEVICE_MOUSE, 1);
        }
    }

    return 1;
}

int xhci_init(void) {
    g_ready = 0;
    g_xhci_controller_count = 0;
    g_last_error = "no xHCI controller";
    g_vendor_id = g_device_id = 0;
    g_pci_bus = g_pci_device = g_pci_function = 0;
    g_bar0 = 0;

    pci_scan();

    for (int i = 0; i < pci_get_device_count(); ++i) {
        const nexus_pci_device_t *dev = pci_get_device(i);
        if (!dev) continue;
        if (dev->class_code != XHCI_CLASS || dev->subclass != XHCI_SUBCLASS || dev->prog_if != XHCI_PROGIF) continue;
        g_xhci_controller_count++;
    }

    if (g_xhci_controller_count == 0) {
        g_last_error = "PCI scan found no xHCI controllers";
        return 0;
    }

    /* Try every xHCI controller, not only the first match. This is important
     * on real Intel systems which can expose two independent USB 3.x host
     * controllers. The first controller that successfully starts becomes the
     * active input backend. */
    int seen = 0;
    for (int i = 0; i < pci_get_device_count(); ++i) {
        const nexus_pci_device_t *dev = pci_get_device(i);
        if (!dev) continue;
        if (dev->class_code != XHCI_CLASS || dev->subclass != XHCI_SUBCLASS || dev->prog_if != XHCI_PROGIF) continue;
        seen++;
        if (xhci_try_controller(dev)) return 1;
        g_ready = 0;
    }

    if (seen > 0 && g_last_error[0] == '\0') g_last_error = "all detected xHCI controllers failed initialization";
    return 0;
}

int xhci_is_ready(void) { return g_ready; }
int xhci_controller_count(void) { return g_xhci_controller_count; }
uint16_t xhci_vendor_id(void) { return g_vendor_id; }
uint16_t xhci_device_id(void) { return g_device_id; }
uint8_t xhci_pci_bus(void) { return g_pci_bus; }
uint8_t xhci_pci_device(void) { return g_pci_device; }
uint8_t xhci_pci_function(void) { return g_pci_function; }
uint64_t xhci_bar0(void) { return g_bar0; }
const char *xhci_last_error(void) { return g_last_error; }
uint8_t xhci_port_count(void) { return g_ports; }
uint8_t xhci_connected_ports(void) { return g_connected; }
int xhci_keyboard_present(void) { return g_kbd_present; }
int xhci_mouse_present(void) { return g_mouse_present; }

void xhci_poll(void) {
    if (!g_ready || (!g_kbd_present && !g_mouse_present)) return;

    for (uint32_t budget = 0; budget < 32; budget++) {
        xhci_trb_t *ev = &g_evt_ring[g_evt_deq];
        uint32_t ctrl = ev->control;
        if (!!(ctrl & TRB_CYCLE) != !!g_evt_cycle) break; /* новых событий нет */

        uint32_t type = TRB_TYPE(ctrl);
        uint64_t evt_deq_addr = (uint64_t)(uintptr_t)&g_evt_ring[g_evt_deq];

        if (type == TRB_TYPE_TRANSFER_EVENT) {
            uint8_t slot = (uint8_t)((ctrl >> 24) & 0xFFu);
            uint8_t dci = (uint8_t)((ctrl >> 16) & 0x1Fu);
            uint32_t completion_code = (ev->status >> 24) & 0xFFu;

            if (slot == g_slot_id && dci == g_kbd_ep_dci &&
                (completion_code == 1 || completion_code == 13)) {
                uint64_t trb_addr = ev->parameter;
                for (int i = 0; i < KBD_RING_TRBS; i++) {
                    if ((uint64_t)(uintptr_t)&g_kbd_ring[i] == trb_addr) {
                        if (g_hid_protocol == 0x01) {
                            process_boot_report(g_kbd_reports[i]);
                            input_record_keyboard(NEXUS_INPUT_SOURCE_USB, 0);
                        } else if (g_hid_protocol == 0x02) {
                            mouse_process_usb_report(g_kbd_reports[i], g_hid_report_len);
                        }
                        kbd_ring_push_normal(i);
                        ring_doorbell(g_slot_id, g_kbd_ep_dci);
                        break;
                    }
                }
            }
        }

        g_evt_deq++;
        if (g_evt_deq == EVT_RING_TRBS) { g_evt_deq = 0; g_evt_cycle ^= 1; }
        mmio_write64(g_rt_base + 0x20 + 0x18, evt_deq_addr | (1u << 3));
    }
}
