#include "vga_terminal.h"

#include "../base/string.h"

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
};

enum {
    VGA_WIDTH = 80,
    VGA_HEIGHT = 25,
    VGA_CELL_COUNT = VGA_WIDTH * VGA_HEIGHT,
};

static volatile uint16_t *const terminal_buffer = (volatile uint16_t *)0xB8000;
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;

static uint8_t vga_entry_color(enum vga_color foreground, enum vga_color background)
{
    return (uint8_t)(foreground | (uint8_t)(background << 4));
}

static uint16_t vga_entry(char character, uint8_t color)
{
    return (uint16_t)(uint8_t)character | (uint16_t)color << 8;
}

static void terminal_clear_row(size_t row)
{
    for (size_t column = 0; column < VGA_WIDTH; column++) {
        terminal_buffer[row * VGA_WIDTH + column] = vga_entry(' ', terminal_color);
    }
}

static void terminal_scroll(void)
{
    for (size_t index = 0; index < VGA_CELL_COUNT - VGA_WIDTH; index++) {
        terminal_buffer[index] = terminal_buffer[index + VGA_WIDTH];
    }

    terminal_clear_row(VGA_HEIGHT - 1);
    terminal_row = VGA_HEIGHT - 1;
}

static void terminal_put_entry(char character, size_t column, size_t row)
{
    if (column < VGA_WIDTH && row < VGA_HEIGHT) {
        terminal_buffer[row * VGA_WIDTH + column] = vga_entry(character, terminal_color);
    }
}

static void terminal_putchar(char character)
{
    if (character == '\0') {
        return;
    }

    if (character == '\n') {
        terminal_column = 0;
        terminal_row++;
    } else if (character == '\r') {
        terminal_column = 0;
    } else if (character == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            terminal_put_entry(' ', terminal_column, terminal_row);
        }
    } else if (character == '\t') {
        size_t spaces = 4 - terminal_column % 4;
        while (spaces-- > 0) {
            terminal_putchar(' ');
        }
    } else {
        terminal_put_entry(character, terminal_column, terminal_row);
        terminal_column++;
        if (terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            terminal_row++;
        }
    }

    if (terminal_row >= VGA_HEIGHT) {
        terminal_scroll();
    }
}

void terminal_initialize(void)
{
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (size_t row = 0; row < VGA_HEIGHT; row++) {
        terminal_clear_row(row);
    }
}

void terminal_set_color(uint8_t color)
{
    terminal_color = color;
}

void terminal_write(const char *data, size_t size)
{
    if (data == NULL) {
        return;
    }

    for (size_t index = 0; index < size; index++) {
        terminal_putchar(data[index]);
    }
}

void terminal_write_string(const char *string)
{
    terminal_write(string, kstrlen(string));
}