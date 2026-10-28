#include "desktop.h"

#include <stdbool.h>
#include <stddef.h>

#include "font.h"
#include "../drivers/keyboard.h"

enum {
    COLOR_BACKGROUND = 0x173B46,
    COLOR_BACKGROUND_LIGHT = 0x245560,
    COLOR_PANEL = 0x102B34,
    COLOR_WINDOW = 0xF0F3F4,
    COLOR_TITLE = 0x17323D,
    COLOR_TEXT = 0x152D34,
    COLOR_MUTED = 0x507078,
    COLOR_ACCENT = 0x37B6A0,
    COLOR_WHITE = 0xFFFFFF,
    TERMINAL_ROWS = 5,
    TERMINAL_COLUMNS = 64,
};

static bool same_text(const char *left, const char *right)
{
    size_t index = 0;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return false;
        }
        index++;
    }
    return left[index] == right[index];
}

static void draw_desktop(struct graphics_framebuffer *framebuffer, bool terminal_open)
{
    graphics_fill_rect(framebuffer, 0, 0, framebuffer->width, framebuffer->height, COLOR_BACKGROUND);
    graphics_fill_rect(framebuffer, 0, 0, framebuffer->width, 34, COLOR_PANEL);
    graphics_fill_rect(framebuffer, 0, framebuffer->height - 42,
        framebuffer->width, 42, COLOR_PANEL);
    graphics_fill_rect(framebuffer, 18, 8, 18, 18, COLOR_ACCENT);
    graphics_draw_text(framebuffer, 45, 13, "KELPOS DESKTOP", COLOR_WHITE, 1);
    graphics_draw_text(framebuffer, framebuffer->width - 145, 13,
        "V-OSDEV", COLOR_WHITE, 1);

    const size_t window_width = framebuffer->width > 900 ? 760 : framebuffer->width - 80;
    const size_t window_height = framebuffer->height > 700 ? 470 : framebuffer->height - 120;
    const size_t window_x = (framebuffer->width - window_width) / 2;
    const size_t window_y = (framebuffer->height - window_height) / 2;
    graphics_fill_rect(framebuffer, window_x, window_y, window_width, window_height, COLOR_WINDOW);
    graphics_fill_rect(framebuffer, window_x, window_y, window_width, 38, COLOR_TITLE);
    graphics_draw_text(framebuffer, window_x + 16, window_y + 15,
        terminal_open ? "KELPOS TERMINAL" : "WELCOME TO KELPOS", COLOR_WHITE, 1);

    if (terminal_open) {
        graphics_draw_text(framebuffer, window_x + 24, window_y + 70,
            "TYPE HELP FOR COMMANDS", COLOR_MUTED, 1);
        graphics_draw_text(framebuffer, window_x + 24, window_y + 100,
            "ESC RETURNS TO DESKTOP", COLOR_MUTED, 1);
        graphics_fill_rect(framebuffer, window_x + 18, window_y + 140,
            window_width - 36, window_height - 165, COLOR_PANEL);
    } else {
        graphics_draw_text(framebuffer, window_x + 28, window_y + 76,
            "YOUR SYSTEM IS READY", COLOR_TEXT, 2);
        graphics_draw_text(framebuffer, window_x + 30, window_y + 120,
            "GRUB STARTED THE KERNEL IN LONG MODE.", COLOR_MUTED, 1);

        const size_t card_y = window_y + 190;
        const size_t card_width = (window_width - 72) / 2;
        graphics_fill_rect(framebuffer, window_x + 24, card_y, card_width, 118, COLOR_WHITE);
        graphics_fill_rect(framebuffer, window_x + 42, card_y + 20, 42, 42, COLOR_ACCENT);
        graphics_draw_text(framebuffer, window_x + 42, card_y + 78, "TERMINAL", COLOR_TEXT, 1);
        graphics_draw_text(framebuffer, window_x + 42, card_y + 96, "PRESS T TO OPEN", COLOR_MUTED, 1);

        graphics_fill_rect(framebuffer, window_x + 48 + card_width, card_y, card_width, 118, COLOR_WHITE);
        graphics_fill_rect(framebuffer, window_x + 66 + card_width, card_y + 20, 42, 42, COLOR_BACKGROUND_LIGHT);
        graphics_draw_text(framebuffer, window_x + 66 + card_width, card_y + 78, "SYSTEM", COLOR_TEXT, 1);
        graphics_draw_text(framebuffer, window_x + 66 + card_width, card_y + 96, "KERNEL BASELINE", COLOR_MUTED, 1);
        graphics_draw_text(framebuffer, window_x + 28, window_y + window_height - 30,
            "T TERMINAL     R REFRESH     Q HALT", COLOR_MUTED, 1);
    }

    graphics_fill_rect(framebuffer, 16, framebuffer->height - 34, 86, 24, COLOR_ACCENT);
    graphics_draw_text(framebuffer, 34, framebuffer->height - 27, "KELP", COLOR_WHITE, 1);
    graphics_draw_text(framebuffer, 120, framebuffer->height - 27,
        terminal_open ? "TERMINAL ACTIVE" : "GRUB MULTIBOOT", COLOR_WHITE, 1);
}

static void draw_terminal_lines(
    struct graphics_framebuffer *framebuffer,
    const char lines[TERMINAL_ROWS][TERMINAL_COLUMNS + 1],
    size_t window_x,
    size_t window_y)
{
    const size_t pitch_width = framebuffer->width > 900 ? 760 : framebuffer->width - 80;
    const size_t terminal_x = window_x + 28;
    const size_t terminal_y = window_y + 154;
    graphics_fill_rect(framebuffer, terminal_x, terminal_y,
        pitch_width - 56, 240, COLOR_PANEL);
    for (size_t row = 0; row < TERMINAL_ROWS; row++) {
        graphics_draw_text(framebuffer, terminal_x + 12, terminal_y + 12 + row * 30,
            lines[row], COLOR_WHITE, 1);
    }
}

static void append_terminal_line(
    char lines[TERMINAL_ROWS][TERMINAL_COLUMNS + 1],
    const char *text)
{
    for (size_t row = 1; row < TERMINAL_ROWS; row++) {
        for (size_t column = 0; column <= TERMINAL_COLUMNS; column++) {
            lines[row - 1][column] = lines[row][column];
        }
    }
    size_t column = 0;
    while (text[column] != '\0' && column < TERMINAL_COLUMNS) {
        lines[TERMINAL_ROWS - 1][column] = text[column];
        column++;
    }
    lines[TERMINAL_ROWS - 1][column] = '\0';
}

static void clear_input(char input[TERMINAL_COLUMNS + 1])
{
    for (size_t index = 0; index <= TERMINAL_COLUMNS; index++) {
        input[index] = '\0';
    }
}

static void execute_terminal_command(
    struct graphics_framebuffer *framebuffer,
    char *command,
    char lines[TERMINAL_ROWS][TERMINAL_COLUMNS + 1])
{
    if (same_text(command, "help")) {
        append_terminal_line(lines, "HELP ABOUT CLEAR ECHO DESKTOP");
    } else if (same_text(command, "about")) {
        append_terminal_line(lines, "KELPOS EARLY DESKTOP SHELL");
    } else if (same_text(command, "clear")) {
        for (size_t row = 0; row < TERMINAL_ROWS; row++) {
            lines[row][0] = '\0';
        }
    } else if (same_text(command, "desktop")) {
        append_terminal_line(lines, "ALREADY ON THE DESKTOP");
    } else if (command[0] == 'e' && command[1] == 'c'
            && command[2] == 'h' && command[3] == 'o'
            && (command[4] == ' ' || command[4] == '\0')) {
        append_terminal_line(lines, command[4] == '\0' ? "" : command + 5);
    } else if (same_text(command, "reboot")) {
        append_terminal_line(lines, "REBOOT NOT IMPLEMENTED YET");
    } else if (command[0] != '\0') {
        append_terminal_line(lines, "UNKNOWN COMMAND - TYPE HELP");
    }

    draw_desktop(framebuffer, true);
    const size_t window_width = framebuffer->width > 900 ? 760 : framebuffer->width - 80;
    const size_t window_height = framebuffer->height > 700 ? 470 : framebuffer->height - 120;
    draw_terminal_lines(framebuffer, lines,
        (framebuffer->width - window_width) / 2,
        (framebuffer->height - window_height) / 2);
}

_Noreturn void desktop_run(struct graphics_framebuffer *framebuffer)
{
    char lines[TERMINAL_ROWS][TERMINAL_COLUMNS + 1];
    char input[TERMINAL_COLUMNS + 1];
    size_t input_length = 0;
    bool terminal_open = false;

    for (size_t row = 0; row < TERMINAL_ROWS; row++) {
        for (size_t column = 0; column <= TERMINAL_COLUMNS; column++) {
            lines[row][column] = '\0';
        }
    }
    append_terminal_line(lines, "KELPOS GRAPHICAL TERMINAL");
    append_terminal_line(lines, "TYPE HELP FOR COMMANDS");

    draw_desktop(framebuffer, false);
    for (;;) {
        char character;
        if (!keyboard_read_char(&character)) {
            __asm__ volatile ("pause");
            continue;
        }

        if (!terminal_open) {
            if (character == 't' || character == 'T') {
                terminal_open = true;
                input_length = 0;
                clear_input(input);
                draw_desktop(framebuffer, true);
                const size_t window_width = framebuffer->width > 900 ? 760 : framebuffer->width - 80;
                const size_t window_height = framebuffer->height > 700 ? 470 : framebuffer->height - 120;
                draw_terminal_lines(framebuffer, lines,
                    (framebuffer->width - window_width) / 2,
                    (framebuffer->height - window_height) / 2);
            } else if (character == 'r' || character == 'R') {
                draw_desktop(framebuffer, false);
            } else if (character == 'q' || character == 'Q') {
                append_terminal_line(lines, "SHUTDOWN NOT IMPLEMENTED");
            }
            continue;
        }

        if (character == 27) {
            terminal_open = false;
            draw_desktop(framebuffer, false);
            continue;
        }
        if (character == '\b') {
            if (input_length > 0) {
                input[--input_length] = '\0';
            }
        } else if (character == '\n') {
            input[input_length] = '\0';
            char echo_line[TERMINAL_COLUMNS + 1];
            echo_line[0] = '>';
            echo_line[1] = ' ';
            echo_line[2] = '\0';
            size_t index = 0;
            while (index < input_length && index + 2 < sizeof(echo_line)) {
                echo_line[index + 2] = input[index];
                index++;
            }
            echo_line[index + 2] = '\0';
            append_terminal_line(lines, echo_line);
            execute_terminal_command(framebuffer, input, lines);
            input_length = 0;
            clear_input(input);
        } else if (character >= ' ' && character <= '~'
                && input_length < TERMINAL_COLUMNS) {
            input[input_length++] = character;
            input[input_length] = '\0';
            char prompt[TERMINAL_COLUMNS + 1];
            prompt[0] = '>';
            prompt[1] = ' ';
            prompt[2] = '\0';
            size_t index = 0;
            while (index < input_length && index + 2 < sizeof(prompt)) {
                prompt[index + 2] = input[index];
                index++;
            }
            prompt[index + 2] = '\0';
            lines[TERMINAL_ROWS - 1][0] = '\0';
            size_t column = 0;
            while (prompt[column] != '\0') {
                lines[TERMINAL_ROWS - 1][column] = prompt[column];
                column++;
            }
            lines[TERMINAL_ROWS - 1][column] = '\0';
            const size_t window_width = framebuffer->width > 900 ? 760 : framebuffer->width - 80;
            const size_t window_height = framebuffer->height > 700 ? 470 : framebuffer->height - 120;
            draw_desktop(framebuffer, true);
            draw_terminal_lines(framebuffer, lines,
                (framebuffer->width - window_width) / 2,
                (framebuffer->height - window_height) / 2);
        }
    }
}