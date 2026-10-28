#ifndef KELPOS_DRIVERS_VGA_TERMINAL_H
#define KELPOS_DRIVERS_VGA_TERMINAL_H

#include <stddef.h>
#include <stdint.h>

void terminal_initialize(void);
void terminal_set_color(uint8_t color);
void terminal_write(const char *data, size_t size);
void terminal_write_string(const char *string);

#endif