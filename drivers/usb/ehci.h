#ifndef NEXUSOS_EHCI_H
#define NEXUSOS_EHCI_H
#include <stdint.h>
int ehci_init(void);
int ehci_is_ready(void);
uint8_t ehci_port_count(void);
uint8_t ehci_connected_ports(void);
#endif
