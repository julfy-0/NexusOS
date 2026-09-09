#ifndef NEXUSOS_XHCI_H
#define NEXUSOS_XHCI_H

#include <stdint.h>

/* USB поверх PCI xHCI (USB 3.x host controller).
 *
 * xhci_init() поднимает контроллер полностью: DCBAA, command ring,
 * event ring, сброс и адресация первого подключённого устройства,
 * получение дескрипторов, поиск HID boot keyboard interface и запуск
 * interrupt IN трансфера для неё. Прерывание от самого контроллера
 * (MSI/legacy IRQ) не используется — вместо этого event ring
 * опрашивается вызовом xhci_poll() на каждый тик PIT (см. idt.c),
 * этого более чем достаточно для клавиатуры (100 Hz против типичного
 * bInterval клавиатуры ~8-10 мс). */

int xhci_init(void);
int xhci_is_ready(void);
uint8_t xhci_port_count(void);
uint8_t xhci_connected_ports(void);

/* 1 если во время init нашлась и настроилась HID boot keyboard. */
int xhci_keyboard_present(void);

/* Вызывать из обработчика IRQ0 (таймер) — не блокируется, только
 * проверяет event ring и, если там появился завершённый interrupt
 * transfer клавиатуры, разбирает HID boot report и отдаёт символы
 * в shell/gui так же, как это делает PS/2 keyboard.c. */
void xhci_poll(void);

#endif
