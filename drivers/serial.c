#include "serial.h"

#include <stdint.h>

#include "x86_io.h"

enum {
    COM1_BASE = 0x3F8,
    SERIAL_DATA = COM1_BASE + 0,
    SERIAL_INTERRUPT_ENABLE = COM1_BASE + 1,
    SERIAL_FIFO_CONTROL = COM1_BASE + 2,
    SERIAL_LINE_CONTROL = COM1_BASE + 3,
    SERIAL_MODEM_CONTROL = COM1_BASE + 4,
    SERIAL_LINE_STATUS = COM1_BASE + 5,
    SERIAL_TRANSMIT_READY = 0x20,
    SERIAL_POLL_LIMIT = 100000,
};

static bool serial_ready;

bool serial_initialize(void)
{
    serial_ready = false;
    x86_port_write8(SERIAL_INTERRUPT_ENABLE, 0x00);
    x86_port_write8(SERIAL_LINE_CONTROL, 0x80);
    x86_port_write8(SERIAL_DATA, 0x03);
    x86_port_write8(SERIAL_INTERRUPT_ENABLE, 0x00);
    x86_port_write8(SERIAL_LINE_CONTROL, 0x03);
    x86_port_write8(SERIAL_FIFO_CONTROL, 0xC7);

    x86_port_write8(SERIAL_MODEM_CONTROL, 0x1E);
    x86_port_write8(SERIAL_DATA, 0xAE);
    if (x86_port_read8(SERIAL_DATA) != 0xAE) {
        x86_port_write8(SERIAL_MODEM_CONTROL, 0x00);
        return false;
    }

    x86_port_write8(SERIAL_MODEM_CONTROL, 0x0F);
    serial_ready = true;
    return true;
}

static void serial_putchar(char character)
{
    if (!serial_ready) {
        return;
    }
    for (unsigned int attempt = 0; attempt < SERIAL_POLL_LIMIT; attempt++) {
        if ((x86_port_read8(SERIAL_LINE_STATUS) & SERIAL_TRANSMIT_READY) != 0) {
            x86_port_write8(SERIAL_DATA, (uint8_t)character);
            return;
        }
    }
}

void serial_write(const char *data, size_t size)
{
    if (data == NULL || !serial_ready) {
        return;
    }
    for (size_t index = 0; index < size; index++) {
        if (data[index] == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(data[index]);
    }
}

void serial_write_string(const char *string)
{
    if (string == NULL) {
        return;
    }
    size_t length = 0;
    while (string[length] != '\0') {
        length++;
    }
    serial_write(string, length);
}