#ifndef NEXUSOS_UHCI_H
#define NEXUSOS_UHCI_H
#include <stdint.h>
int uhci_init(void);
int uhci_is_ready(void);
uint8_t uhci_port_count(void);
uint8_t uhci_connected_ports(void);
#endif
