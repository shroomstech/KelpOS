#ifndef KELPOS_DRIVERS_SERIAL_H
#define KELPOS_DRIVERS_SERIAL_H

#include <stdbool.h>
#include <stddef.h>

bool serial_initialize(void);
void serial_write(const char *data, size_t size);
void serial_write_string(const char *string);

#endif