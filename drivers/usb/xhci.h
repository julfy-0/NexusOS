#ifndef NEXUSOS_XHCI_H
#define NEXUSOS_XHCI_H

#include <stdint.h>

/* USB поверх PCI xHCI (USB 3.x host controller).
 *
 * xhci_init() поднимает контроллер полностью: DCBAA, command ring,
 * event ring, сброс и адресация первого подключённого устройства,
 * получение дескрипторов, поиск HID HID boot keyboard/mouse interface и запуск
 * interrupt IN трансфера для найденного устройства. Прерывание от самого контроллера
 * (MSI/legacy IRQ) не используется — вместо этого event ring
 * опрашивается на каждый таймерный event в kernel_events_process(), а не
 * внутри IRQ-контекста. Этого более чем достаточно для клавиатуры (100 Hz
 * против типичного bInterval клавиатуры ~8-10 мс). */

int xhci_init(void);
int xhci_is_ready(void);
uint8_t xhci_port_count(void);
uint8_t xhci_connected_ports(void);

/* 1 если во время init нашлась и настроилась HID boot keyboard. */
int xhci_keyboard_present(void);
int xhci_mouse_present(void);
int xhci_controller_count(void);
uint16_t xhci_vendor_id(void);
uint16_t xhci_device_id(void);
uint8_t xhci_pci_bus(void);
uint8_t xhci_pci_device(void);
uint8_t xhci_pci_function(void);
uint64_t xhci_bar0(void);
const char *xhci_last_error(void);

/* Вызывается из kernel event loop. Проверяет event ring и, если там
 * появился завершённый interrupt transfer клавиатуры, разбирает HID boot
 * report и отдаёт символы в shell/gui так же, как PS/2 keyboard.c. */
void xhci_poll(void);

#endif
