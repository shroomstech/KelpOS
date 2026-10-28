#ifndef KELPOS_DRIVERS_X86_IO_H
#define KELPOS_DRIVERS_X86_IO_H

#include <stdint.h>

uint8_t x86_port_read8(uint16_t port);
void x86_port_write8(uint16_t port, uint8_t value);

#endif