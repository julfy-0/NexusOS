#include "module.h"
#include "pci.h"
#include "pic.h"
#include "pit.h"
#include "input.h"
#include "keyboard.h"
#include "mouse.h"
#include "network.h"
#include "nvme.h"
#include "ahci.h"
#include "usb.h"
#include "gpu.h"

static int module_pci_init(void) { pci_scan(); return pci_get_device_count() > 0 ? 1 : 1; }
static int module_pic_init(void) { pic_remap(); return 1; }
static int module_pit_init(void) { pit_init(100); return 1; }
static int module_input_init(void) { input_init(); return 1; }
static int module_keyboard_init(void) { keyboard_init(); return 1; }
static int module_mouse_init(void) { mouse_init(); return 1; }
static int module_network_init(void) { nexus_network_init(); return 1; }
static int module_nvme_init(void) { return nvme_init(); }
static int module_ahci_init(void) { return ahci_init(); }
static int module_usb_init(void) { return usb_init(); }
static int module_gpu_init(void) { return gpu_init(); }

static void module_nop_exit(void) { }

NEXUS_MODULE("pci", "1.0", NEXUS_MODULE_DRIVER, 100, module_pci_init, module_nop_exit);
NEXUS_MODULE("pic", "1.0", NEXUS_MODULE_DRIVER, 110, module_pic_init, module_nop_exit);
NEXUS_MODULE("pit", "1.0", NEXUS_MODULE_DRIVER, 120, module_pit_init, module_nop_exit);
NEXUS_MODULE("input", "1.0", NEXUS_MODULE_DRIVER, 130, module_input_init, module_nop_exit);
NEXUS_MODULE("keyboard", "1.0", NEXUS_MODULE_DRIVER, 200, module_keyboard_init, module_nop_exit);
NEXUS_MODULE("mouse", "1.0", NEXUS_MODULE_DRIVER, 210, module_mouse_init, module_nop_exit);
NEXUS_MODULE("network", "1.0", NEXUS_MODULE_DRIVER, 300, module_network_init, module_nop_exit);
NEXUS_MODULE("nvme", "1.0", NEXUS_MODULE_DRIVER, 310, module_nvme_init, module_nop_exit);
NEXUS_MODULE("ahci", "1.0", NEXUS_MODULE_DRIVER, 320, module_ahci_init, module_nop_exit);
NEXUS_MODULE("usb", "1.0", NEXUS_MODULE_DRIVER, 400, module_usb_init, module_nop_exit);
NEXUS_MODULE("gpu", "1.0", NEXUS_MODULE_DRIVER, 410, module_gpu_init, module_nop_exit);
