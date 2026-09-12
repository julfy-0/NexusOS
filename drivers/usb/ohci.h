#ifndef NEXUSOS_OHCI_H
#define NEXUSOS_OHCI_H
#include <stdint.h>
int ohci_init(void);
int ohci_is_ready(void);
uint8_t ohci_port_count(void);
uint8_t ohci_connected_ports(void);
#endif
