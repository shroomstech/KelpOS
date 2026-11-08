#include "desktop.h"

#include <stdbool.h>
#include <stddef.h>

#include "font.h"
#include "../drivers/keyboard.h"

enum {
    BACKGROUND = 0x163942,
    PANEL = 0x102831,
    WINDOW = 0xE9F0EF,
    ACCENT = 0x37B6A0,
    INK = 0x19343C,
    WHITE = 0xFFFFFF,
    ROWS = 6,
    COLUMNS = 48,
};

static bool equal(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void add_line(char lines[ROWS][COLUMNS + 1], const char *text)
{
    for (size_t row = 1; row < ROWS; row++) {
        for (size_t column = 0; column <= COLUMNS; column++) {
            lines[row - 1][column] = lines[row][column];
        }
    }
    size_t column = 0;
    while (column < COLUMNS && text[column]) {
        lines[ROWS - 1][column] = text[column];
        column++;
    }
    lines[ROWS - 1][column] = '\0';
}

static void command_run(char lines[ROWS][COLUMNS + 1], const char *input)
{
    if (equal(input, "help")) {
        add_line(lines, "HELP  ABOUT  ECHO  CLEAR  DESKTOP");
    } else if (equal(input, "about")) {
        add_line(lines, "KELPOS - LIGHTWEIGHT DESKTOP");
    } else if (equal(input, "clear")) {
        for (size_t row = 0; row < ROWS; row++) {
            lines[row][0] = '\0';
        }
    } else if (equal(input, "desktop")) {
        add_line(lines, "PRESS ESC TO CLOSE THE TERMINAL");
    } else if (input[0] == 'e' && input[1] == 'c' && input[2] == 'h'
            && input[3] == 'o' && (input[4] == ' ' || !input[4])) {
        add_line(lines, input[4] ? input + 5 : "");
    } else if (input[0]) {
        add_line(lines, "UNKNOWN COMMAND - TYPE HELP");
    }
}

/* A single framebuffer, a panel, and a keyboard-driven terminal. No heap or compositor. */
void desktop_draw(struct graphics_framebuffer *fb, bool terminal_open,
    const char *const lines[6], const char *input)
{
    if (fb == NULL || fb->pixels == NULL || fb->width < 320 || fb->height < 240) {
        return;
    }

    graphics_fill_rect(fb, 0, 0, fb->width, fb->height, BACKGROUND);
    graphics_fill_rect(fb, 0, 0, fb->width, 32, PANEL);
    graphics_fill_rect(fb, 12, 8, 16, 16, ACCENT);
    graphics_draw_text(fb, 38, 12, "KELPOS", WHITE, 1);

    const size_t bottom = fb->height - 36;
    graphics_fill_rect(fb, 0, bottom, fb->width, 36, PANEL);
    graphics_fill_rect(fb, 12, bottom + 7, 80, 22, ACCENT);
    graphics_draw_text(fb, 29, bottom + 14, "KELP", PANEL, 1);
    graphics_draw_text(fb, 108, bottom + 14,
        terminal_open ? "TERMINAL  |  ESC CLOSE" : "T TERMINAL  |  R REDRAW", WHITE, 1);

    const size_t width = fb->width > 700 ? 600 : fb->width - 40;
    const size_t height = fb->height > 520 ? 360 : fb->height - 90;
    const size_t x = (fb->width - width) / 2;
    const size_t y = 44 + (fb->height - 80 - height) / 2;
    graphics_fill_rect(fb, x, y, width, height, WINDOW);
    graphics_fill_rect(fb, x, y, width, 30, PANEL);
    graphics_draw_text(fb, x + 12, y + 11,
        terminal_open ? "TERMINAL" : "WELCOME", WHITE, 1);

    if (!terminal_open) {
        graphics_draw_text(fb, x + 20, y + 55, "WELCOME TO KELPOS", INK, 1);
        graphics_draw_text(fb, x + 20, y + 80, "A SIMPLE, LIGHTWEIGHT DESKTOP", INK, 1);
        graphics_fill_rect(fb, x + 20, y + 115, 56, 56, ACCENT);
        graphics_draw_text(fb, x + 28, y + 138, ">_", PANEL, 1);
        graphics_draw_text(fb, x + 90, y + 133, "PRESS T FOR TERMINAL", INK, 1);
    } else {
        graphics_fill_rect(fb, x + 12, y + 38, width - 24, height - 50, PANEL);
        /* Keep history above the prompt even when GRUB selects a small mode. */
        size_t visible = (height - 100) / 20;
        if (visible > ROWS) {
            visible = ROWS;
        }
        for (size_t row = 0; row < visible; row++) {
            graphics_draw_text(fb, x + 20, y + 50 + row * 20,
                lines[ROWS - visible + row], WHITE, 1);
        }
        graphics_draw_text(fb, x + 20, y + height - 32, ">", ACCENT, 1);
        graphics_draw_text(fb, x + 34, y + height - 32, input, WHITE, 1);
    }
}

_Noreturn void desktop_run(struct graphics_framebuffer *framebuffer)
{
    char lines[ROWS][COLUMNS + 1];
    char input[COLUMNS + 1];

    // Manuelles Nullsetzen statt = {{0}} / = {0} (verhindert memset-Aufruf)
    for (size_t row = 0; row < ROWS; row++) {
        for (size_t col = 0; col <= COLUMNS; col++) {
            lines[row][col] = '\0';
        }
    }
    for (size_t col = 0; col <= COLUMNS; col++) {
        input[col] = '\0';
    }

    size_t length = 0;
    bool terminal_open = false;

    add_line(lines, "KELPOS TERMINAL READY");
    add_line(lines, "TYPE HELP FOR COMMANDS");
    for (;;) {
        const char *display_lines[ROWS];
        for (size_t row = 0; row < ROWS; row++) {
            display_lines[row] = lines[row];
        }
        desktop_draw(framebuffer, terminal_open, display_lines, input);

        char key;
        do {
            if (!keyboard_read_char(&key)) {
                __asm__ volatile ("pause");
            } else {
                break;
            }
        } while (true);

        if (!terminal_open) {
            if (key == 't' || key == 'T') {
                terminal_open = true;
            }
            continue;
        }
        if (key == 27) {
            terminal_open = false;
        } else if (key == '\b') {
            if (length) {
                input[--length] = '\0';
            }
        } else if (key == '\n') {
            add_line(lines, input);
            command_run(lines, input);
            length = 0;
            input[0] = '\0';
        } else if (key >= ' ' && key <= '~' && length < COLUMNS - 2) {
            input[length++] = key;
            input[length] = '\0';
        }
    }
}
