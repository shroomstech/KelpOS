#include "shell.h"

#include <stdbool.h>
#include <stddef.h>

#include "../drivers/keyboard.h"
#include "../drivers/vga_terminal.h"
#include "../memory/heap.h"

enum { SHELL_LINE_CAPACITY = 128 };

static bool text_equals(const char *left, const char *right)
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

static const char *skip_spaces(const char *text)
{
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return text;
}

static void execute_command(char *line)
{
    char *argument = line;
    while (*argument != '\0' && *argument != ' ' && *argument != '\t') {
        argument++;
    }
    if (*argument != '\0') {
        *argument++ = '\0';
    }
    argument = (char *)skip_spaces(argument);

    if (*line == '\0') {
        return;
    }
    if (text_equals(line, "help")) {
        terminal_write_string("Commands: help, about, clear, echo, mem, desktop\n");
    } else if (text_equals(line, "about")) {
        terminal_write_string("KelpOS early kernel TTY shell. No userspace yet.\n");
    } else if (text_equals(line, "clear")) {
        terminal_initialize();
    } else if (text_equals(line, "echo")) {
        terminal_write_string(argument);
        terminal_write_string("\n");
    } else if (text_equals(line, "mem")) {
        terminal_write_string("Bootstrap heap free bytes: ");
        /* Print decimal without pulling in a hosted printf implementation. */
        char digits[24];
        size_t count = 0;
        size_t available = kernel_heap_available();
        do {
            digits[count++] = (char)('0' + available % 10);
            available /= 10;
        } while (available != 0 && count < sizeof(digits));
        while (count > 0) {
            terminal_write(&digits[--count], 1);
        }
        terminal_write_string("\n");
    } else if (text_equals(line, "desktop")) {
        terminal_write_string("Reboot and choose KelpOS Desktop in GRUB.\n");
    } else {
        terminal_write_string("Unknown command: ");
        terminal_write_string(line);
        terminal_write_string("\nType 'help' for commands.\n");
    }
}

_Noreturn void shell_run(void)
{
    char line[SHELL_LINE_CAPACITY];
    size_t length = 0;
    terminal_write_string("\nKelpOS TTY shell\nType 'help' to list commands.\n");

    for (;;) {
        terminal_write_string("kelp> ");
        length = 0;

        for (;;) {
            char character;
            if (!keyboard_read_char(&character)) {
                __asm__ volatile ("pause");
                continue;
            }

            if (character == '\n') {
                terminal_write_string("\n");
                line[length] = '\0';
                execute_command(line);
                break;
            }
            if (character == '\b') {
                if (length > 0) {
                    length--;
                    terminal_write_string("\b");
                }
                continue;
            }
            if (character < ' ' || character > '~' || length + 1 >= sizeof(line)) {
                continue;
            }

            line[length++] = character;
            terminal_write(&character, 1);
        }
    }
}